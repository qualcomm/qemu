#!/usr/bin/env bash

./scripts/checkpatch.pl $(git merge-base upstream/master HEAD)..HEAD
