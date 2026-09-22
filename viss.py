#!/usr/bin/env python3
"""
Viss Programming Language CLI
Version 0.0.1.2
"""
import sys
import os

if __name__ == "__main__":
    src_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "src")
    sys.path.insert(0, src_dir)
    import vissc
    vissc.main()
