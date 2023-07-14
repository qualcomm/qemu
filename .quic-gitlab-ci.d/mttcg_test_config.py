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
    "caffe-cnns_resnet-50_w8_a8": {
        "check": ["sha256sum", "--status", "--check", "ref_{test_name}.sha256"],
        "prog-args": [],
        "qemu-args": [
            "-append",
            "pickle_driver.elf --pickle_in "
            "{test_name}.pickle --in_bhwd "
            "1,224,224,3 --in_elsizes 1 "
            "--out_elsizes 1 --num_ins 1 "
            "--num_outs 1 --out_bhwd "
            "1,1,1,1000 --out "
            "{test_name}.out --in "
            "{test_name}.0.dat --perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2",
        "test-program": "booter",
    },
    "mobilenet_v1_quant": {
        "check": ["sha256sum", "--status", "--check", "ref_{test_name}.sha256"],
        "prog-args": [],
        "qemu-args": [
            "-append",
            "pickle_driver.elf --pickle_in "
            "{test_name}.pickle --in_bhwd "
            "1,224,224,3 --out_elsizes 1 --out_bhwd "
            "1,1,1,1001 --out {test_name}.out --in "
            "{test_name}.0.dat --perf 0 --warmup_run "
            "0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2",
        "test-program": "booter",
    },
    "qnn_delegate_aib5_mobilenet_v3_quant_504": {
        "check": ["sha256sum", "--status", "--check", "ref_{test_name}.sha256"],
        "prog-args": [],
        "qemu-args": [
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "1,512,512,3 "
            "--in_elsizes 1 "
            "--out_elsizes 1 "
            "--num_ins 1 "
            "--num_outs 1 "
            "--out_bhwd "
            "1,1,1,1001 "
            "--setopt "
            "fold_relu_flag 0 "
            "--setopt "
            "hmx_short_conv_flag "
            "0 --out "
            "{test_name}.out "
            "--in "
            "{test_name}.0.dat "
            "--perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2",
        "test-program": "booter",
    },
}
