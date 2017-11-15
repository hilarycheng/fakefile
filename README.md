# fakefile

When we backup file (Linux/Unix) into CIFS share, CIFS cannot handle the device file, symlink and owner permissions. But fakeroot/pseudo cannot store the symlinks into local storage and only store the root owner. We wants to preserve all owner, permissions, special device node on the CIFS/Non-Unix filesystem without implements a linux virtual filesystem.

