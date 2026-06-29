# Copyright(c) 2026 Qualcomm Innovation Center, Inc. All Rights Reserved.
# SPDX-License-Identifier: GPL-2.0-or-later

{
    "mobilenet_v1_quant": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf --pickle_in "
            "{test_name}.pickle --in_bhwd "
            "1,224,224,3 --out_elsizes 1 --out_bhwd "
            "1,1,1,1001 --out {test_name}.out --in "
            "{test_name}.0.dat --perf 0 --warmup_run "
            "0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "caffe-cnns_inception_v4_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle --in_bhwd "
            "1,299,299,3 --in_elsizes 1 "
            "--out_elsizes 1 --num_ins 1 "
            "--num_outs 1 --out_bhwd "
            "1,1,1,1000 --out "
            "{test_name}.out --in "
            "{test_name}.0.dat --perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "caffe-cnns_resnet-50_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
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
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "qnn_delegate_aib5_crnn_float": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf --pickle_in "
            "{test_name}.pickle --in_bhwd "
            "1,64,200,3 --in_elsizes 4 "
            "--out_elsizes 4 --out_bhwd "
            "1,1,1,1800 --setopt "
            "relaxed_precision_flag 1 "
            "--out {test_name}.out --in "
            "{test_name}.0.dat --perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "qnn_delegate_aib5_mobilenet_v2_b8_float": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "8,224,224,3 "
            "--in_elsizes 4 "
            "--out_elsizes 4 "
            "--num_ins 1 "
            "--num_outs 1 "
            "--out_bhwd "
            "8,1,1,1001 "
            "--setopt "
            "relaxed_precision_flag "
            "1 --setopt "
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
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "qnn_delegate_aib5_mobilenet_v2_b8_quant_504": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "8,224,224,3 "
            "--in_elsizes 1 "
            "--out_elsizes "
            "1 --num_ins 1 "
            "--num_outs 1 "
            "--out_bhwd "
            "8,1,1,1001 "
            "--setopt "
            "fold_relu_flag "
            "0 --setopt "
            "hmx_short_conv_flag "
            "0 --out "
            "{test_name}.out "
            "--in "
            "{test_name}.0.dat "
            "--perf 0 "
            "--warmup_run "
            "0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "qnn_delegate_aib5_mobilenet_v3_quant_504": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
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
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "tflite-custom_aib4_vgg19_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd 1,256,256,1 "
            "--out_elsizes 1 --out_bhwd "
            "1,256,256,1 --out "
            "{test_name}.out --in "
            "{test_name}.0.dat --perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "onnx19-cnns_yolo_v5_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf --pickle_in "
            "{test_name}.pickle --in_bhwd "
            "1,640,640,3 --in_elsizes 1 "
            "--out_elsizes 1,1,1 --num_ins 1 "
            "--num_outs 3 --out_bhwd "
            "1,80,80,255:1,40,40,255:1,20,20,255 "
            "--out {test_name}.0.out --in "
            "{test_name}.0.dat --perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "tf2_10-cnns_mobilenet_v1_quantaware_fp": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "1,224,224,3 "
            "--in_elsizes 4 "
            "--out_elsizes 4 "
            "--num_ins 1 "
            "--num_outs 1 "
            "--out_bhwd "
            "1,1,1,1001 --setopt "
            "relaxed_precision_flag "
            "1 --out "
            "{test_name}.out "
            "--in "
            "{test_name}.0.dat "
            "--perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "onnx16-custom_auto_uc_mvp_1308_1_0_convnext_ptq_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "1,224,224,3 "
            "--in_elsizes "
            "1 "
            "--out_elsizes "
            "1 "
            "--num_ins "
            "1 "
            "--num_outs "
            "1 "
            "--out_bhwd "
            "1,1,1,1000 "
            "--out "
            "{test_name}.out "
            "--in "
            "{test_name}.0.dat "
            "--perf "
            "0 "
            "--warmup_run "
            "0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "onnx16-cnns_efficientnet_lite_bn_fold_16bit_w8_a16": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "1,224,224,3 "
            "--in_elsizes "
            "2 "
            "--out_elsizes "
            "2 "
            "--num_ins "
            "1 "
            "--num_outs "
            "1 "
            "--out_bhwd "
            "1,1,1,1000 "
            "--out "
            "{test_name}.out "
            "--in "
            "{test_name}.0.dat "
            "--perf "
            "0 "
            "--warmup_run "
            "0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "tf2_10-custom_mlperf_mobilebert_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "1,1,384,1:1,1,1,384:1,1,1,384 "
            "--in_elsizes 4,4,4 "
            "--out_elsizes 1 "
            "--num_ins 3 "
            "--num_outs 1 "
            "--out_bhwd 1,2,1,384 "
            "--out "
            "{test_name}.out --in "
            "{test_name}.0.dat,{test_name}.1.dat,{test_name}.2.dat "
            "--perf 0 "
            "--warmup_run 0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "onnx19-custom_uc_honor_1777_1_0_vitbase_without_5d_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "1,224,224,3 "
            "--in_elsizes "
            "1 "
            "--out_elsizes "
            "1 "
            "--num_ins "
            "1 "
            "--num_outs "
            "1 "
            "--out_bhwd "
            "1,1,1,256 "
            "--out "
            "{test_name}.out "
            "--in "
            "{test_name}.0.dat "
            "--perf "
            "0 "
            "--warmup_run "
            "0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
    "onnx16-custom_uc_honor_1776_1_0_mobilevit_v1xxs_fixed112_w8_a8": {
        "check": [
            "sha256sum",
            "--status",
            "--check",
            "ref_{test_name}.sha256",
        ],
        "prog-args": [],
        "qemu-args": [
            "-M",
            "V81QA_1",
            "-cpu",
            "v81",
            "-append",
            "pickle_driver.elf "
            "--pickle_in "
            "{test_name}.pickle "
            "--in_bhwd "
            "1,112,112,3 "
            "--in_elsizes "
            "1 "
            "--out_elsizes "
            "1 "
            "--num_ins "
            "1 "
            "--num_outs "
            "1 "
            "--out_bhwd "
            "1,1,1,1000 "
            "--out "
            "{test_name}.out "
            "--in "
            "{test_name}.0.dat "
            "--perf "
            "0 "
            "--warmup_run "
            "0",
        ],
        "qemu-program": "qemu-system-hexagon",
        "test-dir": "nn/hexagon-nn-v3/h2_v81",
        "test-program": "booter",
    },
}
