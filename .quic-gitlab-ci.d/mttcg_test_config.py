{
    'snpe-m10': {
        'qemu-program': 'qemu-system-hexagon',
        'test-dir': 'nn/snpe/m10/',
        'test-program': 'runelf.pbn',
        'prog-args': [],
        'qemu-args': ['-M', 'V68N_1024', '-append',
            './run_main_on_hexagon_sim -- SnpeDspEmulatorDriver_q.so --inputDataPath ./inputs',
            '-cpu', 'any,usefs={test_dir},sched-limit=on', ],
        'check': None,
    },
    'gzip': {
        'qemu-program': 'qemu-system-hexagon',
        'test-dir': 'apps/gzip',
        'test-program': 'gzip',
        'prog-args': [],
        'qemu-args': [],
        'check': ['sha256sum', '--status', '--check', 'ref.sha256',],
    },
    'H2-linux': {
        'qemu-program': 'qemu-system-hexagon',
        'test-dir': 'OS/h2_linux/',
        'test-program': 'loadlinux.v68',
        'prog-args': [],
        'qemu-args': ['-M', 'V66_Linux', '-cpu', 'any,sched-limit=on',],
        'check': None,
    },
    "libcxx-alg_sorting": {
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "apps/libcxx_test",
        "test-program": "../QuRT_SDK/sdkv75/runelf.pbn",
        "prog-args": [
            "-append",
            "../QuRT_SDK/sdkv75/run_main_on_hexagon_sim -- ./alg.sorting.complexity.exe",
        ],
        "qemu-args": ["-cpu", "any,usefs=../QuRT_SDK/sdkv75/"],
        "check": None,
    },
    "libcxx-regex_exponential": {
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "apps/libcxx_test",
        "test-program": "../QuRT_SDK/sdkv75/runelf.pbn",
        "prog-args": [
            "-append",
            "../QuRT_SDK/sdkv75/run_main_on_hexagon_sim -- ./regex_exponential.exe",
        ],
        "qemu-args": ["-cpu", "any,usefs=../QuRT_SDK/sdkv75/"],
        "check": None,
    },
    "libcxx-next_prime": {
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "apps/libcxx_test",
        "test-program": "../QuRT_SDK/sdkv75/runelf.pbn",
        "prog-args": [
            "-append",
            "../QuRT_SDK/sdkv75/run_main_on_hexagon_sim -- ./next_prime.exe",
        ],
        "qemu-args": ["-cpu", "any,usefs=../QuRT_SDK/sdkv75/"],
        "check": None,
    },
    "h264-butterfly": {
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "apps/h264",
        "test-program": "h264",
        "prog-args": [
            "-append",
            "-b 4 8 -t 2 -in ./Butterfly_30_qvga.264 "
            "-o ./Butterfly_30_qvga.yuv -s appstats.txt",
        ],
        "qemu-args": [],
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref.sha256",
        ],
    },
}
