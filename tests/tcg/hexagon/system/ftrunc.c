/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>



int main() {

  size_t rc;
  int fp;
  struct stat st;
  char *fname = {"_testfile_ftrunc"};

  memset(&st, 0, sizeof(struct stat));
  if ((rc = stat(fname, &st)) != 0) {
    perror("stat");
    printf("FAIL: rc = %d\n", rc);
    return 1;
  }
  assert (st.st_size == 6);
  time_t orig_mod_time = st.st_mtime;

  assert (st.st_atime != 0);
  assert (st.st_mtime != 0);
  assert (st.st_ctime != 0);


  if (!(fp = open(fname, O_RDWR))) {
    perror("open");
    return 1;
  }
  if ((rc = ftruncate(fp, 1)) != 0) {
    perror("ftruncate");
    printf("FAIL: rc = %d\n", rc);
    return 1;
  }

  memset(&st, 0, sizeof(struct stat));
  if ((rc = fstat(fp, &st)) != 0) {
    perror("fstat");
    printf("FAIL: rc = %d\n", rc);
    return 1;
  }
  assert (st.st_size == 1);
  assert (st.st_atime != 0);
  assert (st.st_mtime != 0);
  assert (st.st_ctime != 0);
  assert (st.st_mtime != orig_mod_time);

  close(fp);
  return 0;
}
