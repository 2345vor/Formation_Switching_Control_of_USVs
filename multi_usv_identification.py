"""
Multi-USV Pooled Identification: Train on USV1 + USV3, Validate on USV2
=======================================================================

Pipeline
--------
1) Per-boat preprocessing (demo2 style):
       - x, y, Heading -> psi (deg -> rad, unwrapped)
       - psi  -> r   (np.gradient)
       - u, v already in the data; we still smooth u, v, r by Savitzky-Golay
       - finite difference -> du, dv, dr
       - PWM centering  (PWM - 1500)
2) Per-boat normalization (StandardScaler on [Tp, Ts] only; states NOT normalized)
3) Build per-boat regression matrices  (block-diagonal, 3N x 21)
4) Pool  USV1 + USV3  -> joint dataset
5) SLSQP with physics-based box constraints  (eq.15 in the paper)
6) Cross-validation:
       self-prediction (train -> train boats) and cross-prediction
       (train -> USV2) at the regression level (normalized MSE)
7) Trajectory-level simulation: feed model into kinematics,
       compare with USV2 measurement (this is the actual generalization test)
8) Figures that match the multi_usv_identification scheme

Author: Mavis (refactored from multi_usv_identification + demo2)
"""

import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
from matplotlib.colors import LinearSegmentedColormap
from scipy.optimize import minimize
from scipy.signal import savgol_filter
from sklearn.preprocessing import StandardScaler
import seaborn as sns


# ============================================================
# 1. Configuration
# ============================================================
DATA_DIR   = 'data'
BOAT_FILES  = ['usv1_data.xlsx', 'usv2_data.xlsx', 'usv3_data.xlsx']
BOAT_NAMES  = ['USV1', 'USV2', 'USV3']
N_BOATS    = len(BOAT_FILES)

# Train boats: USV1 (index 0) + USV3 (index 2); test boat: USV2 (index 1)
TRAIN_IDX   = [0, 2]
TEST_IDX    = 1

START_ROW   = 0
N_ROWS      = 450
DT          = 0.1                   # sampling period, s
SAVGOL_WIN  = 5                    # 0.5 s @ 0.1 s
SAVGOL_POLY = 2
PWM_CENTER  = 1500.0               # demo2 style centering
N_PARAMS    = 21                   # 3 DOF * 7 coefficients

# physics-based box constraints (a1..a7, b1..b7, c1..c7)
PARAM_BOUNDS = [
    (-10,  10),   # a1  v*r
    (-10,   0),   # a2  surge linear damping (must be negative)
    (-10,  10),   # a3  v in surge
    (-10,  10),   # a4  r in surge
    (  0,  0.1),   # a5  Tp in surge (positive)
    (  0,  0.1),   # a6  Ts in surge (positive)
    (-0.1,  0.1),   # a7  bias
    (-10,  10),   # b1  u*r
    (-10,  10),   # b2  u in sway
    (-10,   0),   # b3  sway linear damping (must be negative)
    (-10,  10),   # b4  r in sway
    (-0.001,   0),   # b5  Tp in sway
    (  0,  0.001),   # b6  Ts in sway
    (-0.01,  0.01),   # b7  bias
    (-100,  100),   # c1  u*v
    (-10,  10),   # c2  u in yaw
    (-10,  10),   # c3  v in yaw
    (-10,   0),   # c4  yaw linear damping (must be negative)
    (-10,   0),   # c5  Tp in yaw
    (  0,  10),   # c6  Ts in yaw
    (-10,  10),   # c7  bias
]

# initial guess (same as multi_usv_identification / demo2)
INITIAL_PARAMS = np.array([
    -1.1391,  0.0028,  0.6836,  0.6836,  0.6836,  0.6836,  0.6836,   # a1..a7
     0.0161, -0.0052,  0.0020,  0.0068,  0.0020,  0.0068,  0.0161,   # b1..b7
     8.2861, -0.9860,  0.0307,  1.3276,  0.0307,  1.3276,  0.0010    # c1..c7
])

RNG = np.random.default_rng(0)


# ============================================================
# 2. Per-boat preprocessing (demo2-style + Savitzky-Golay)
# ============================================================
def unwrap_angle(psi_deg):
    """deg -> rad, unwrap to remove [-pi, pi] jumps."""
    return np.unwrap(psi_deg * np.pi / 180.0)


def preprocess_boat(df, dt=DT):
    """Return dict of smoothed states and finite-difference derivatives."""
    x       = df['x'].values
    y       = df['y'].values
    psi_deg = df['Heading'].values
    Ts_raw  = df['PWM_R'].values
    Tp_raw  = df['PWM_L'].values
    u_raw   = df['u'].values
    v_raw   = df['v'].values

    psi = unwrap_angle(psi_deg)
    r   = np.gradient(psi, dt)

    # demo2: smooth u, v, r; we use polyorder=2 to match the paper
    u = savgol_filter(u_raw, SAVGOL_WIN, SAVGOL_POLY)
    v = savgol_filter(v_raw, SAVGOL_WIN, SAVGOL_POLY)
    r = savgol_filter(r,       SAVGOL_WIN, SAVGOL_POLY)

    # finite-difference derivatives (paper eq.12)
    du = np.gradient(u, dt)
    dv = np.gradient(v, dt)
    dr = np.gradient(r, dt)

    return {
        'x': x, 'y': y, 'psi': psi,
        'u': u, 'v': v, 'r': r,
        'du': du, 'dv': dv, 'dr': dr,
        # demo2 style: center PWM at 1500
        'Tp': Tp_raw - PWM_CENTER,
        'Ts': Ts_raw - PWM_CENTER,
        'N':  len(x),
    }


# ============================================================
# 3. Per-boat normalization (paper eq.13)
# ============================================================
def normalize_boat(data):
    """StandardScaler on [Tp, Ts] only. States (u,v,r,du,dv,dr) NOT normalized."""
    ctrl_cols = ['Tp', 'Ts']
    stack = np.column_stack([data[c] for c in ctrl_cols])
    sc    = StandardScaler()
    norm  = sc.fit_transform(stack)
    out   = {}
    # states: keep original values (not normalized)
    for c in ['u', 'v', 'r', 'du', 'dv', 'dr']:
        out[c] = data[c]
    # controls: normalized
    for i, c in enumerate(ctrl_cols):
        out[c] = norm[:, i]
    out['x']    = data['x']
    out['y']    = data['y']
    out['psi']  = data['psi']
    out['N']    = data['N']
    out['scaler'] = sc
    return out


# ============================================================
# 4. Regression matrix for a single boat
# ============================================================
def build_regression(data_norm):
    """
    Build block-diagonal feature matrix (3N x 21) and target vector (3N,).
        du = a1 v r + a2 u + a3 v + a4 r + a5 Tp + a6 Ts + a7
        dv = b1 u r + b2 u + b3 v + b4 r + b5 Tp + b6 Ts + b7
        dr = c1 u v + c2 u + c3 v + c4 r + c5 Tp + c6 Ts + c7
    """
    n = data_norm['N'] - 1                       # drop last sample (FD alignment)
    u  = data_norm['u'][:-1]
    v  = data_norm['v'][:-1]
    r  = data_norm['r'][:-1]
    Tp = data_norm['Tp'][:-1]
    Ts = data_norm['Ts'][:-1]
    du = data_norm['du'][1:]
    dv = data_norm['dv'][1:]
    dr = data_norm['dr'][1:]

    X_du = np.column_stack([v * r, u, v, r, Tp, Ts, np.ones(n)])
    X_dv = np.column_stack([u * r, u, v, r, Tp, Ts, np.ones(n)])
    X_dr = np.column_stack([u * v, u, v, r, Tp, Ts, np.ones(n)])

    X = np.zeros((3 * n, N_PARAMS))
    X[0       * n:1 * n,  0: 7] = X_du
    X[1       * n:2 * n,  7:14] = X_dv
    X[2       * n:3 * n, 14:21] = X_dr

    y = np.concatenate([du, dv, dr])
    return X, y, n


# ============================================================
# 5. Loss + model equations (in normalized space)
# ============================================================
def model_equations(params, X, U):
    """Used by the un-normalized demo2-style simulation (kept for reference)."""
    a, b, c = params[0:7], params[7:14], params[14:21]
    u_vec, v_vec, r_vec = X[:, 0], X[:, 1], X[:, 2]
    Ts_vec, Tp_vec      = U[:, 0], U[:, 1]
    du = a[0] * v_vec * r_vec + a[1] * u_vec + a[2] * v_vec + a[3] * r_vec \
         + a[4] * Tp_vec + a[5] * Ts_vec + a[6]
    dv = b[0] * u_vec * r_vec + b[1] * u_vec + b[2] * v_vec + b[3] * r_vec \
         + b[4] * Tp_vec + b[5] * Ts_vec + b[6]
    dr = c[0] * u_vec * v_vec + c[1] * u_vec + c[2] * v_vec + c[3] * r_vec \
         + c[4] * Tp_vec + c[5] * Ts_vec + c[6]
    return np.column_stack((du, dv, dr))


def loss_regression(params, X, y):
    """Sum of squared residuals in normalized space."""
    return float(np.sum((X @ params - y) ** 2))


# ============================================================
# 6. Predict derivatives in *original* units (for trajectory sim)
# ============================================================
def predict_physical(params, u, v, r, Tp, Ts, scaler):
    """Forward-pass: normalize Tp/Ts with `scaler`, apply model. States NOT normalized."""
    mu    = scaler.mean_
    sigma = scaler.scale_

    # scaler has 2 columns: [Tp, Ts]
    Tp_n = (Tp - mu[0]) / sigma[0]
    Ts_n = (Ts - mu[1]) / sigma[1]

    a, b, c = params[0:7], params[7:14], params[14:21]
    du = a[0] * v * r + a[1] * u + a[2] * v + a[3] * r \
         + a[4] * Tp_n + a[5] * Ts_n + a[6]
    dv = b[0] * u * r + b[1] * u + b[2] * v + b[3] * r \
         + b[4] * Tp_n + b[5] * Ts_n + b[6]
    dr = c[0] * u * v + c[1] * u + c[2] * v + c[3] * r \
         + c[4] * Tp_n + c[5] * Ts_n + c[6]
    return du, dv, dr


def simulate_with_params(params, raw, scaler, dt, N):
    """Integrate the identified model and the kinematic equations."""
    u = np.zeros(N); v = np.zeros(N); r = np.zeros(N)
    x = np.zeros(N); y = np.zeros(N); psi = np.zeros(N)
    u[0], v[0], r[0]     = raw['u'][0], raw['v'][0], raw['r'][0]
    x[0], y[0], psi[0]   = raw['x'][0], raw['y'][0], raw['psi'][0]

    for k in range(1, N):
        Ts = raw['Ts'][k - 1]
        Tp = raw['Tp'][k - 1]
        uc, vc, rc, psic = u[k-1], v[k-1], r[k-1], psi[k-1]

        du, dv, dr = predict_physical(params, uc, vc, rc, Tp, Ts, scaler)

        u[k] = uc + dt * du
        v[k] = vc + dt * dv
        r[k] = rc + dt * dr
        x[k] = x[k-1] + dt * (uc * np.cos(psic) - vc * np.sin(psic))
        y[k] = y[k-1] + dt * (uc * np.sin(psic) + vc * np.cos(psic))
        psi[k] = (psic + dt * rc + np.pi) % (2 * np.pi) - np.pi

    return x, y, psi, u, v, r


# ============================================================
# 7. Per-channel regression MSE (paper eq.16)
# ============================================================
def regression_mse(theta, X, y, n):
    pred = X @ theta
    return {
        'du':   float(np.mean((pred[0       * n:1 * n] - y[0       * n:1 * n]) ** 2)),
        'dv':   float(np.mean((pred[1       * n:2 * n] - y[1       * n:2 * n]) ** 2)),
        'dr':   float(np.mean((pred[2       * n:3 * n] - y[2       * n:3 * n]) ** 2)),
    }


# ============================================================
# 8. Cross-validation helper
# ============================================================
def run_cv_fold(train_idx, test_idx, raw, norm, X_list, y_list, n_list):
    """Train on train_idx boats, test on test_idx boat.
    Returns dict with train/test boat names, theta, and trajectory MSEs."""
    train_names = [BOAT_NAMES[k] for k in train_idx]
    test_name   = BOAT_NAMES[test_idx]
    
    # Pool training data
    X_train = np.vstack([X_list[k] for k in train_idx])
    y_train = np.concatenate([y_list[k] for k in train_idx])
    
    # SLSQP optimization
    res = minimize(
        loss_regression, INITIAL_PARAMS,
        args=(X_train, y_train),
        method='SLSQP', bounds=PARAM_BOUNDS,
        options={'disp': False, 'maxiter': 2000, 'ftol': 1e-10},
    )
    theta_fold = res.x
    
    # Self-simulation on each training boat -> (x, y, psi) RMSE
    self_mse = {}
    for k in train_idx:
        d  = raw[k]
        ns = norm[k]['scaler']
        Ns = d['N']
        xs, ys, psis, _, _, _ = simulate_with_params(theta_fold, d, ns, DT, Ns)
        self_mse[BOAT_NAMES[k]] = {
            'x':   float(np.sqrt(np.mean((xs - d['x']) ** 2))),
            'y':   float(np.sqrt(np.mean((ys - d['y']) ** 2))),
            'psi': float(np.sqrt(np.mean((np.unwrap(psis) - np.unwrap(d['psi'])) ** 2))),
        }
    
    # Cross-simulation on test boat -> (x, y, psi) RMSE
    d_test  = raw[test_idx]
    ns_test = norm[test_idx]['scaler']
    Ns_test = d_test['N']
    xs_t, ys_t, psis_t, us_t, vs_t, rs_t = simulate_with_params(
        theta_fold, d_test, ns_test, DT, Ns_test
    )
    test_mse = {
        'x':   float(np.sqrt(np.mean((xs_t - d_test['x']) ** 2))),
        'y':   float(np.sqrt(np.mean((ys_t - d_test['y']) ** 2))),
        'psi': float(np.sqrt(np.mean((np.unwrap(psis_t) - np.unwrap(d_test['psi'])) ** 2))),
    }
    
    # Full simulation data for plotting (return for the reference fold)
    sim_data = (xs_t, ys_t, psis_t, us_t, vs_t, rs_t)
    
    return {
        'train_names': train_names,
        'test_name':   test_name,
        'theta':       theta_fold,
        'self_mse':    self_mse,
        'test_mse':    test_mse,
        'sim_data':    sim_data,
        'res':         res,
    }


# ============================================================
# 9. Driver
# ============================================================
def main():
    print('=' * 70)
    print('Multi-USV Pooled Identification  |  3-Fold Cross-Validation')
    print('=' * 70)

    # ----- 1. load and preprocess -----
    raw, norm = [], []
    print('\n[1] Per-boat preprocessing ...')
    for i, fn in enumerate(BOAT_FILES):
        path = os.path.join(DATA_DIR, fn)
        df   = pd.read_excel(path).iloc[START_ROW:START_ROW + N_ROWS]
        d    = preprocess_boat(df, dt=DT)
        n    = normalize_boat(d)
        raw.append(d); norm.append(n)
        sc = n['scaler']
        print(f'  {BOAT_NAMES[i]:<4}  N={d["N"]:4d}  '
              f'u in [{d["u"].min():+.3f}, {d["u"].max():+.3f}]   '
              f'v in [{d["v"].min():+.3f}, {d["v"].max():+.3f}]   '
              f'r in [{d["r"].min():+.3f}, {d["r"].max():+.3f}]')

    # ----- 2. per-boat regression matrices -----
    print('\n[2] Per-boat regression matrices ...')
    X_list, y_list, n_list = [], [], []
    for i in range(N_BOATS):
        X, y, n = build_regression(norm[i])
        X_list.append(X); y_list.append(y); n_list.append(n)
        print(f'  {BOAT_NAMES[i]:<4}  per-channel n={n}  ->  regression rows={3*n}')

    # ----- 3. 3-fold cross-validation -----
    folds = [
        {'train': [1, 2], 'test': 0},  # USV2+USV3 -> USV1
        {'train': [0, 2], 'test': 1},  # USV1+USV3 -> USV2
        {'train': [0, 1], 'test': 2},  # USV1+USV2 -> USV3
    ]
    
    cv_results = []
    print('\n[3] Running 3-fold cross-validation ...')
    for fi, fold in enumerate(folds):
        train_idx = fold['train']
        test_idx  = fold['test']
        train_str = '+'.join(BOAT_NAMES[k] for k in train_idx)
        test_str  = BOAT_NAMES[test_idx]
        print(f'  Fold {fi+1}: Train [{train_str}] -> Test [{test_str}] ...')
        
        result = run_cv_fold(train_idx, test_idx, raw, norm, X_list, y_list, n_list)
        cv_results.append(result)
        
        if result['res'].success:
            print(f'    Converged. Loss = {result["res"].fun:.6f}')
        else:
            print(f'    WARNING: {result["res"].message}')
    
    # ----- 4. print cross-validation table (LaTeX table format) -----
    print('\n[4] Cross-validation results: x / y / psi RMSE')
    print('=' * 140)
    print(f'  {"训练船":<14}{"测试船":<8}{"x MSE":>28}{"y MSE":>28}{"psi MSE":>28}{"综合Score":>28}')
    print('-' * 140)
    
    best_fold = None
    best_score = float('inf')
    
    for result in cv_results:
        train_str = '+'.join(result['train_names'])
        test_str  = result['test_name']
        
        # Self-prediction values in train order
        self_x_vals   = [result['self_mse'][name]['x']   for name in result['train_names']]
        self_y_vals   = [result['self_mse'][name]['y']   for name in result['train_names']]
        self_psi_vals = [result['self_mse'][name]['psi'] for name in result['train_names']]
        
        # Calculate test MSE values
        test_x = result['test_mse']['x']
        test_y = result['test_mse']['y']
        test_psi = result['test_mse']['psi']
        
        # Calculate composite score (lower is better)
        score = test_x + test_y + test_psi  # Simple sum of MSEs
        
        # Track best fold
        if score < best_score:
            best_score = score
            best_fold = result
        
        x_str   = f'{test_x:.3f} ({self_x_vals[0]:.3f},{self_x_vals[1]:.3f})'
        y_str   = f'{test_y:.3f} ({self_y_vals[0]:.3f},{self_y_vals[1]:.3f})'
        psi_str = f'{test_psi:.3f} ({self_psi_vals[0]:.3f},{self_psi_vals[1]:.3f})'
        
        print(f'  {train_str:<14}{test_str:<8}{x_str:>28}{y_str:>28}{psi_str:>28}{score:>28.3f}')
    
    # Print best fold
    if best_fold:
        print('-' * 140)
        best_train = '+'.join(best_fold['train_names'])
        best_test = best_fold['test_name']
        print(f'  [Best] {best_train} -> {best_test}, Score = {best_score:.3f}')
    
    print('-' * 110)
    print('  注：括号内为训练船的自预测 MSE，按训练船顺序列出。')
    print('=' * 110)
    
    # ----- 5. print identified model for each fold -----
    print('\n[5] Identified models for each fold:')
    for fi, result in enumerate(cv_results):
        train_str = '+'.join(result['train_names'])
        theta_fold = result['theta']
        a, b, c = theta_fold[0:7], theta_fold[7:14], theta_fold[14:21]
        print(f'\n  Fold {fi+1} ({train_str} -> {result["test_name"]}):')
        print(f'    du = {a[0]:+.4f}*v*r + {a[1]:+.4f}*u + {a[2]:+.4f}*v + {a[3]:+.4f}*r '
              f'+ {a[4]:+.4f}*Tp + {a[5]:+.4f}*Ts + {a[6]:+.4f}')
        print(f'    dv = {b[0]:+.4f}*u*r + {b[1]:+.4f}*u + {b[2]:+.4f}*v + {b[3]:+.4f}*r '
              f'+ {b[4]:+.4f}*Tp + {b[5]:+.4f}*Ts + {b[6]:+.4f}')
        print(f'    dr = {c[0]:+.4f}*u*v + {c[1]:+.4f}*u + {c[2]:+.4f}*v + {c[3]:+.4f}*r '
              f'+ {c[4]:+.4f}*Tp + {c[5]:+.4f}*Ts + {c[6]:+.4f}')

    # ----- 6. plot using fold 1 (USV2+USV3 -> USV1) as reference -----
    ref_result = cv_results[0]  # Fold 1: USV2+USV3 -> USV1
    theta = ref_result['theta']
    
    # Build self_sim for plotting
    self_sim = {}
    for k in [1, 2]:  # USV2, USV3
        d  = raw[k]
        ns = norm[k]['scaler']
        Ns = d['N']
        xsk, ysk, _, usk, vsk, rsk = simulate_with_params(theta, d, ns, DT, Ns)
        self_sim[BOAT_NAMES[k]] = {
            'pos_mean': float(np.mean(np.sqrt((xsk - d['x']) ** 2 + (ysk - d['y']) ** 2))),
            'pos_max':  float(np.max (np.sqrt((xsk - d['x']) ** 2 + (ysk - d['y']) ** 2))),
            'u_rmse':   float(np.sqrt(np.mean((usk - d['u']) ** 2))),
            'v_rmse':   float(np.sqrt(np.mean((vsk - d['v']) ** 2))),
            'r_rmse':   float(np.sqrt(np.mean((rsk - d['r']) ** 2))),
        }
    
    xs_t, ys_t, psis_t, us_t, vs_t, rs_t = ref_result['sim_data']
    d_test = raw[0]  # USV1
    Nsim = d_test['N']
    time = np.arange(Nsim) * DT
    pos_err = np.sqrt((xs_t - d_test['x']) ** 2 + (ys_t - d_test['y']) ** 2)
    rmse = {
        'x':  float(np.sqrt(np.mean((xs_t - d_test['x']) ** 2))),
        'y':  float(np.sqrt(np.mean((ys_t - d_test['y']) ** 2))),
        'u':  float(np.sqrt(np.mean((us_t - d_test['u']) ** 2))),
        'v':  float(np.sqrt(np.mean((vs_t - d_test['v']) ** 2))),
        'r':  float(np.sqrt(np.mean((rs_t - d_test['r']) ** 2))),
        'psi':float(np.sqrt(np.mean((np.unwrap(psis_t) - np.unwrap(d_test['psi'])) ** 2))),
        'pos_mean': float(np.mean(pos_err)),
        'pos_max':  float(np.max(pos_err)),
    }

    # Build cv_table for plotting compatibility
    cv_table = {}
    for k in [1, 2]:
        cv_table[BOAT_NAMES[k]] = {
            'role': 'train',
            'mse':  regression_mse(theta, X_list[k], y_list[k], n_list[k]),
        }
    cv_table[BOAT_NAMES[0]] = {
        'role': 'test',
        'mse':  regression_mse(theta, X_list[0], y_list[0], n_list[0]),
    }

    # Extract the three thetas from cv_results for comparison heatmap
    cv_thetas = [r['theta'] for r in cv_results]

    # ----- 7. plot everything -----
    print('\n[6] Plotting figures (reference fold: USV1+USV3 -> USV2) ...')
    figures = make_all_figures(
        raw=raw, norm=norm, theta=theta,
        X_list=X_list, y_list=y_list, n_list=n_list,
        cv_table=cv_table, self_sim=self_sim,
        usv2_sim=(xs_t, ys_t, psis_t, us_t, vs_t, rs_t),
        rmse=rmse, time=time, dt=DT,
        cv_thetas=cv_thetas,
    )
    for f in figures:
        f.show()

    print('\n=== Done ===')


# ============================================================
# 9. Figures  (multi_usv_identification style: Figure A-G)
# ============================================================
def make_all_figures(raw, norm, theta, X_list, y_list, n_list,
                     cv_table, self_sim, usv2_sim, rmse, time, dt,
                     cv_thetas=None):
    figs = []

    colors = {'USV1': '#1f77b4', 'USV2': '#d62728', 'USV3': '#2ca02c'}
    color_list = ['C0', 'C1', 'C2']  # Use matplotlib default colors

    # ---- Figure A: USV1~3 trajectory display (measured + simulated) ----
    fA, axA = plt.subplots(figsize=(7, 5))
    for i in range(N_BOATS):
        d = raw[i]
        ns = norm[i]['scaler']
        Ns = d['N']
        x_sim, y_sim, _, _, _, _ = simulate_with_params(theta, d, ns, dt, Ns)
        axA.plot(d['x'], d['y'], color=color_list[i], linestyle='-', linewidth=1.5,
                 label=f'{BOAT_NAMES[i]} measured')
        axA.plot(x_sim, y_sim, color=color_list[i], linestyle='--', linewidth=1.5,
                 label=f'{BOAT_NAMES[i]} simulated')
    axA.set_xlabel('x (m)'); axA.set_ylabel('y (m)')
    axA.axis('equal')
    axA.legend(loc='best', fontsize=8, ncol=2)
    fA.tight_layout(); figs.append(fA)

    # ---- Figure B: 4x1 plot of u, v, r, psi for all 3 USVs ----
    fB, axesB = plt.subplots(4, 1, figsize=(8, 10))
    for i in range(N_BOATS):
        d = raw[i]
        ns = norm[i]['scaler']
        Ns = d['N']
        x_sim, y_sim, psi_sim, u_sim, v_sim, r_sim = simulate_with_params(theta, d, ns, dt, Ns)
        t = np.arange(Ns) * dt

        axesB[0].plot(t, d['u'], color=color_list[i], linestyle='-', linewidth=1.2)
        axesB[0].plot(t, u_sim, color=color_list[i], linestyle='--', linewidth=1.2)

        axesB[1].plot(t, d['v'], color=color_list[i], linestyle='-', linewidth=1.2)
        axesB[1].plot(t, v_sim, color=color_list[i], linestyle='--', linewidth=1.2)

        axesB[2].plot(t, d['r'], color=color_list[i], linestyle='-', linewidth=1.2)
        axesB[2].plot(t, r_sim, color=color_list[i], linestyle='--', linewidth=1.2)

        axesB[3].plot(t, np.unwrap(d['psi']), color=color_list[i], linestyle='-', linewidth=1.2)
        axesB[3].plot(t, np.unwrap(psi_sim), color=color_list[i], linestyle='--', linewidth=1.2)

    axesB[0].set_ylabel('$u$ (m/s)')
    axesB[1].set_ylabel('$v$ (m/s)')
    axesB[2].set_ylabel('$r$ (rad/s)')
    axesB[3].set_xlabel('$t$ (s)'); axesB[3].set_ylabel('$\\psi$ (rad)')

    # common legend for all subplots
    from matplotlib.lines import Line2D
    legend_elements = []
    for i in range(N_BOATS):
        legend_elements.append(Line2D([0], [0], color=color_list[i], linestyle='-',
                                      linewidth=1.2, label=f'{BOAT_NAMES[i]} measured'))
        legend_elements.append(Line2D([0], [0], color=color_list[i], linestyle='--',
                                      linewidth=1.2, label=f'{BOAT_NAMES[i]} simulated'))
    fB.legend(handles=legend_elements, loc='upper center', bbox_to_anchor=(0.5, 0.84),
              ncol=3, fontsize=8, frameon=False)

    fB.tight_layout(); figs.append(fB)

    # ---- Figure C: Original trajectories (with labels, sparser ticks) ----
    fC, axC = plt.subplots(figsize=(7, 5))
    for i in range(N_BOATS):
        axC.plot(raw[i]['x'], raw[i]['y'], color=color_list[i], linestyle='-', linewidth=2,
                 label=BOAT_NAMES[i])
    axC.set_xlabel('x (m)', fontsize=9); axC.set_ylabel('y (m)', fontsize=9)
    axC.axis('equal')
    axC.xaxis.set_major_locator(MaxNLocator(4))
    axC.yaxis.set_major_locator(MaxNLocator(4))
    axC.legend(loc='best', fontsize=9)
    fC.tight_layout(); figs.append(fC)

    # ---- Figure D: 3-fold parameter comparison heatmap (horizontal, side-by-side) ----
    # Model equations:
    # du = a1*v*r + a2*u + a3*v + a4*r + a5*Tp + a6*Ts + a7
    # dv = b1*u*r + b2*u + b3*v + b4*r + b5*Tp + b6*Ts + b7  
    # dr = c1*u*v + c2*u + c3*v + c4*r + c5*Tp + c6*Ts + c7
    fold_labels = ['Fold 1: USV2+USV3 → USV1',
                   'Fold 2: USV1+USV3 → USV2',
                   'Fold 3: USV1+USV2 → USV3']
    thetas_to_plot = [theta] * 3  # default fallback
    if cv_thetas is not None:
        thetas_to_plot = cv_thetas

    colors_hm = [(0.2, 0.6, 0.9), (1, 1, 1), (0.9, 0.2, 0.2)]
    cmap_hm = LinearSegmentedColormap.from_list('custom_cmap', colors_hm, N=256)

    state_labels = [r'$\dot{u}$', r'$\dot{v}$', r'$\dot{r}$']
    param_names_per_row = [
        ['v*r', 'u', 'v', 'r', 'Tp', 'Ts', '1'],  # du
        ['u*r', 'u', 'v', 'r', 'Tp', 'Ts', '1'],  # dv
        ['u*v', 'u', 'v', 'r', 'Tp', 'Ts', '1'],  # dr
    ]

    fD, axesD = plt.subplots(3, 1, figsize=(10, 14))

    # Calculate unified color range across all folds
    all_values = []
    for fi in range(3):
        theta_i = thetas_to_plot[fi]
        all_values.extend(theta_i)
    vmin = np.min(all_values)
    vmax = np.max(all_values)

    # Create shared colorbar axes
    from mpl_toolkits.axes_grid1 import make_axes_locatable
    divider = make_axes_locatable(axesD[-1])
    cax = divider.append_axes("right", size="5%", pad=0.5)

    for fi in range(3):
        theta_i = thetas_to_plot[fi]
        matrix_h = np.zeros((3, 7))
        for j in range(7):
            for i in range(3):
                matrix_h[i, j] = theta_i[i * 7 + j]

        annotations_h = np.empty((3, 7), dtype=object)
        for i in range(3):
            for j in range(7):
                val = theta_i[i * 7 + j]
                if abs(val) >= 100:
                    formatted_value = f"{val:.1f}"
                elif abs(val) >= 10:
                    formatted_value = f"{val:.2f}"
                elif abs(val) >= 0.1:
                    formatted_value = f"{val:.3f}"
                else:
                    formatted_value = f"{val:.4f}"
                annotations_h[i, j] = f"{formatted_value}*\n{param_names_per_row[i][j]}"

        # Only show colorbar on the last axis
        cbar = True if fi == 2 else False
        sns.heatmap(matrix_h, ax=axesD[fi], annot=annotations_h, fmt='',
                    annot_kws={'fontsize': 10}, cmap=cmap_hm,
                    vmin=vmin, vmax=vmax,
                    cbar=cbar, cbar_ax=cax if fi == 2 else None,
                    cbar_kws={'label': 'Param'},
                    xticklabels=[r'$\theta_{%d}$' % (j + 1) for j in range(7)],
                    yticklabels=state_labels,
                    linewidths=0.5, linecolor='#e6e6e6')
        axesD[fi].set_title(fold_labels[fi], fontsize=12, fontweight='bold')
        axesD[fi].set_xticklabels([r'$\theta_{%d}$' % (j + 1) for j in range(7)], fontsize=10)
        axesD[fi].set_yticklabels(state_labels, rotation=0, fontsize=10)

    fD.tight_layout(); figs.append(fD)

    # ---- Figure E: Heatmap of the unified 21-parameter model (vertical) ----
    fE, axE = plt.subplots(figsize=(5, 7))
    
    matrix_v = np.zeros((7, 3))
    for j in range(3):
        for i in range(7):
            matrix_v[i, j] = theta[j * 7 + i]

    annotations_v = np.empty((7, 3), dtype=object)
    for j in range(3):  # derivatives
        for i in range(7):  # parameters
            val = theta[j * 7 + i]
            if abs(val) >= 100:
                formatted_value = f"{val:.1f}"
            elif abs(val) >= 10:
                formatted_value = f"{val:.2f}"
            elif abs(val) >= 0.1:
                formatted_value = f"{val:.3f}"
            else:
                formatted_value = f"{val:.4f}"
            annotations_v[i, j] = f"{formatted_value}*\n{param_names_per_row[j][i]}"

    sns.heatmap(matrix_v, ax=axE, annot=annotations_v, fmt='',
                annot_kws={'fontsize': 12}, cmap=cmap_hm,
                vmin=vmin, vmax=vmax,  # Use the same color range as Figure D
                cbar_kws={'label': 'Parameter Value'},
                xticklabels=state_labels,
                yticklabels=[r'$\theta_{%d}$' % (i + 1) for i in range(7)],
                linewidths=0.5, linecolor='#e6e6e6')
    axE.set_xticklabels(state_labels, fontsize=12)
    axE.set_yticklabels([r'$\theta_{%d}$' % (i + 1) for i in range(7)], rotation=0, fontsize=12)
    
    if axE.collections and hasattr(axE.collections[0], 'colorbar') and axE.collections[0].colorbar:
        cbar = axE.collections[0].colorbar
        cbar.ax.yaxis.label.set_size(12)
        cbar.ax.tick_params(labelsize=12)
    
    fE.tight_layout(); figs.append(fE)

    # ---- Figure F: Training signals (u, v, r, Tp, Ts) — first 100 rows, xy swapped, side-by-side ----
    N_plot = 100
    fF, axesF = plt.subplots(1, 5, figsize=(10, 3.5))
    signal_names = ['u', 'v', 'r', 'Tp', 'Ts']
    colors_F = {'USV1': 'C0', 'USV3': 'C1'}
    time_F = np.arange(N_plot) * dt

    for j, sig in enumerate(signal_names):
        for idx in TRAIN_IDX:
            tag = BOAT_NAMES[idx]
            axesF[j].plot(raw[idx][sig][:N_plot], time_F,
                          color=colors_F[tag], linewidth=2.5)
        axesF[j].set_xticks([])
        axesF[j].set_yticks([])
        axesF[j].invert_yaxis()
    fF.tight_layout(pad=0.5); figs.append(fF)

    # ---- Figure G: Training derivatives (du, dv, dr) — first 100 rows, xy swapped, side-by-side ----
    fG, axesG = plt.subplots(1, 3, figsize=(6, 3.5))
    deriv_names = ['du', 'dv', 'dr']
    colors_G = {'USV1': 'C0', 'USV3': 'C1'}

    for j, sig in enumerate(deriv_names):
        for idx in TRAIN_IDX:
            tag = BOAT_NAMES[idx]
            axesG[j].plot(raw[idx][sig][:N_plot], time_F,
                          color=colors_G[tag], linewidth=2.5)
        axesG[j].set_xticks([])
        axesG[j].set_yticks([])
        axesG[j].invert_yaxis()
    fG.tight_layout(pad=0.5); figs.append(fG)
    
    plt.show()

    return figs


# ============================================================
if __name__ == '__main__':
    main()
