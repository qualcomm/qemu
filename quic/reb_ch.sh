#!/bin/bash

THIS_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# TODO: 686 build

git rebase -i \
    -x ${THIS_DIR}/gitexec.sh \
    -x ${THIS_DIR}/gitexec-clang.sh \
    -x ${THIS_DIR}/gitexec-noidef.sh \
    --signoff \
    tip
