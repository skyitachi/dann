import argparse
import sys
from pathlib import Path


def _require_deps():
    try:
        import h5py  # noqa: F401
        import numpy as np  # noqa: F401
        import faiss  # noqa: F401
    except Exception as e:
        raise RuntimeError(
            "Missing Python deps. Install: pip install -U numpy h5py faiss-cpu (or faiss-gpu)"
        ) from e


def _pick_dataset(f, preferred_names, dim=None):
    for name in preferred_names:
        if name in f:
            ds = f[name]
            if hasattr(ds, "shape") and len(ds.shape) == 2 and (dim is None or ds.shape[1] == dim):
                return name

    candidates = []
    def _visit(prefix, obj):
        if hasattr(obj, "shape") and hasattr(obj, "dtype"):
            if len(obj.shape) == 2 and (dim is None or obj.shape[1] == dim):
                candidates.append(prefix)

    f.visititems(_visit)
    if not candidates:
        raise RuntimeError("No 2D vector dataset found in hdf5")
    candidates.sort()
    return candidates[0]


def main():
    _require_deps()
    import h5py
    import numpy as np
    import faiss

    ap = argparse.ArgumentParser()
    ap.add_argument("--hdf5", default="data/nytimes-256-angular.hdf5")
    ap.add_argument("--train-dataset", default="")
    ap.add_argument("--out", default="data/nytimes_hnsw_ip.index")
    ap.add_argument("--m", type=int, default=32)
    ap.add_argument("--ef-construction", type=int, default=200)
    ap.add_argument("--batch", type=int, default=50000)
    ap.add_argument("--dim", type=int, default=256)
    args = ap.parse_args()

    hdf5_path = Path(args.hdf5)
    out_path = Path(args.out)

    if not hdf5_path.exists():
        raise FileNotFoundError(str(hdf5_path))

    with h5py.File(hdf5_path, "r") as f:
        if args.train_dataset:
            train_name = args.train_dataset
        else:
            train_name = _pick_dataset(f, ["train", "base", "dataset", "vectors"], dim=args.dim)

        train = f[train_name]
        if len(train.shape) != 2 or train.shape[1] != args.dim:
            raise RuntimeError(f"Train dataset {train_name} shape {train.shape} != (N, {args.dim})")

        n, d = int(train.shape[0]), int(train.shape[1])

        index = faiss.IndexHNSWFlat(d, args.m, faiss.METRIC_INNER_PRODUCT)
        index.hnsw.efConstruction = args.ef_construction

        for start in range(0, n, args.batch):
            end = min(n, start + args.batch)
            xb = np.asarray(train[start:end], dtype=np.float32)
            faiss.normalize_L2(xb)
            index.add(xb)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    faiss.write_index(index, str(out_path))
    print(f"saved_index={out_path}")
    print(f"ntotal={index.ntotal}")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
