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
    ap.add_argument("--test-dataset", default="")
    ap.add_argument("--index", default="data/nytimes_hnsw_ip.index")
    ap.add_argument("--k", type=int, default=10)
    ap.add_argument("--nq", type=int, default=10)
    ap.add_argument("--ef-search", type=int, default=64)
    ap.add_argument("--dim", type=int, default=256)
    args = ap.parse_args()

    hdf5_path = Path(args.hdf5)
    index_path = Path(args.index)

    if not hdf5_path.exists():
        raise FileNotFoundError(str(hdf5_path))
    if not index_path.exists():
        raise FileNotFoundError(str(index_path))

    index = faiss.read_index(str(index_path))
    if hasattr(index, "hnsw"):
        index.hnsw.efSearch = args.ef_search

    with h5py.File(hdf5_path, "r") as f:
        if args.test_dataset:
            test_name = args.test_dataset
        else:
            test_name = _pick_dataset(f, ["test", "query", "queries"], dim=args.dim)

        test = f[test_name]
        if len(test.shape) != 2 or test.shape[1] != args.dim:
            raise RuntimeError(f"Test dataset {test_name} shape {test.shape} != (N, {args.dim})")

        nq = min(args.nq, int(test.shape[0]))
        xq = np.asarray(test[:nq], dtype=np.float32)
        faiss.normalize_L2(xq)

    D, I = index.search(xq, args.k)

    print(f"index={index_path}")
    print(f"nq={xq.shape[0]} k={args.k}")
    print("I:")
    print(I)
    print("D:")
    print(D)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
