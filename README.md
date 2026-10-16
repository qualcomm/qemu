QEMU CI
=======

This repository is a downstream fork of
[QEMU](https://gitlab.com/qemu-project/qemu), testing all patches posted on
[mailing list](https://lore.kernel.org/qemu-devel).

All series are pushed as branches (using message-id as name), and tested.
As well, master branch is continuously updated to match upstream.

Series status can be checked on
[this page](https://github.com/p-b-o/qemu-ci/branches/all).

Disclaimer: This is not a project supported by upstream QEMU. It's provided as a
convenience to help you check status of series and apply easily an existing
series by fetching associated branch.

---

To use it on your fork:

```
git remote add qemu-ci https://github.com/p-b-o/qemu-ci
git fetch qemu-ci master
git cherry-pick qemu-ci/master
# and enable Actions on your GitHub repository
```

Note: qemu-ci/master is continuously updated with hot fixes for CI, so it's
better to reapply it every time you rebase your personal branch.

---

[CI yaml file](https://github.com/p-b-o/qemu-ci/blob/ci/.github/workflows/build.yml)
has been written to be self contained, to use containers and to be as explicit
as possible. As a result, it is fairly easy to take any command and run it on
your machine.

In case you want to reproduce the exact same environment as GitHub, you can use
[github-runners](https://github.com/second-reality/github-runners), which
provides convenient ssh access to all runners GitHub offers.
