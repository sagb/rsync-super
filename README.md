# Rsync-super

![Rsync-Super Explained](explained.jpg)

### An rsync patch allowing chroot in SSH (non-daemon) mode.

It adds six **mandatory** arguments, which must be specified first and in fixed order:

```sh
rsync \
--chroot-dir directory \
--chroot-uid numeric_user_id \
--chroot-gid numeric_group_id
```

On startup, rsync immediately and unconditionally changes the root to the specified
directory and drops privileges.

On failure to do that, it exits.


## Usage

- You must understand what you are doing and why you need it.
  The SUID rsync without careful planning is **dangerous**.
  Read the "Rationale" and "Security Considerations" below.

- Collect the following data:

  + `LOGIN_UID`, `LOGIN_USERNAME`: a user ID and name of the user who will 
    log in via SSH. You may choose a generic name for `LOGIN_USERNAME` like "rsync"
    or "user". Only a single SSH user 
    is allowed, though it can be switched later to multiple UIDs using
    `.authorized_keys`, see `TEST_CHROOT_UID`.
    Restrict `LOGIN_UID` from any other activity on the server.
    If somebody can run rsync-super from under this UID with arbitrary arguments,
    not controlled by `.authorized_keys` restrictions - you are in trouble.

  + `LOGIN_GID`: a group ID of the aforementioned user. Only this user must be a member
    of this group.

  + `EXE_PATH`: a path to the rsync executable.

  + `TEST_CHROOT_UID`: user ID of one of your real users, let's say "customers".
    They may be all the same as `LOGIN_UID`, but may be different as well.

  + `TEST_CHROOT_GID`: group ID of the aforementioned customer.

  + `TEST_CHROOT_KEY`: a public SSH key of the aforementioned customer: "ssh-rsa ..."

  + `TEST_CHROOT_PRIVKEY_PATH`: a path to the corresponding private key.

  + `TEST_CHROOT_DIR`: a chroot area for the aforementioned customer.
    It's best to start with an empty directory, belonging to the user,
    with permissions `700`.

- Prepare a [build environment for rsync](https://github.com/RsyncProject/rsync/blob/master/INSTALL.md)

- Build `rsync-super`:
  ```sh
  ./update \
  --with-allowed-uid=$LOGIN_UID \
  --enable-chroot-verbose \
  ...other-configure-options-for-rsync...
  ```
  Skip `--enable-chroot-verbose` in production.

  For details about the `update` script, read the header comments [inside](update) it.
  Alternatively, you can manually apply patches from the `patches/` directory
  and copy files from the `src/` directory.

- Transfer the `rsync` executable to `EXE_PATH` on your rsync server.

- Make it SUID and executable only by the `LOGIN_GID` group:
```sh
chown root:$LOGIN_GID $EXE_PATH
chmod 4750 $EXE_PATH
```

- Add to `LOGIN_USERNAME`'s `~/.ssh/authorized_keys` (manually replace all variables
with their values):
```
restrict,command="EXE_PATH --chroot-dir TEST_CHROOT_DIR --chroot-uid TEST_CHROOT_UID --chroot-gid TEST_CHROOT_GID" ssh-rsa TEST_CHROOT_KEY
```

- That's all. Try to transfer something on behalf of the test customer:
```sh
rsync -ve "ssh -i $TEST_CHROOT_PRIVKEY_PATH" test_file $LOGIN_USERNAME@your.server:/
```
Note that `LOGIN_USERNAME` is the same for all customers.  
Note the "/". The file should appear under the customer's chroot.

- If something goes wrong, allow `--enable-chroot-verbose` in configure.
  On any failure in chroot code, rsync exits with code 60.


## Rationale

This fork allows for a multiuser rsync server in a modern zero-trust environment,
with three features simultaneously:

- Clean chroot, not clogged by executables and their libraries.

- Clean SSH authentication, without rsync daemon passwords or "chaining" rsync to rsync.

- Not relying on filtering user input with restricted shells or rrsync, because of the complexity of rsync features and options, and possibly introducing additional attack vectors.

Only [any two](https://en.wikipedia.org/wiki/Inconsistent_triad) of these 
features are achievable using traditional rsync with or without any helper tools.

Another explanation is [here](https://github.com/RsyncProject/rsync/discussions/662).


## Security Considerations

- Rsync-super starts as an SUID binary, i.e., jumps to **super**user, until the early chroot.
**Any user who is allowed to run rsync-super with arbitrary arguments
has full control over the system.**  
To partially mitigate this, rsync-super accepts only a single `LOGIN_UID`
and refuses `--chroot-uid` or `--chroot-gid` of less than 1000.  
Try hard to prevent `LOGIN_UID` from running arbitrary commands,
i.e., from escaping `.authorized_keys` restrictions.

- Without option filtering,
rsync can do anything with symlinks, devices, and it can start a daemon.
So it's up to you to ensure that the `--chroot-uid` either has no capabilities 
or the action will not cause harm.  
Don't put any special files or mounts in the chroot.  
Mount the chroot itself with `noexec` and `nodev`.  
Avoid moving directories to/from the chroot while rsync is running.  
Ideally, don't share a chroot between multiple customers, because there is
a great risk that they will be able to impersonate each other.  


### Risk Mitigation in the Code

- Three `--chroot-*` arguments are deliberately positional and mandatory,
  for bullet-proof parsing.

- Chroot is performed right at the very start of the executable,
  to skip running complicated setup code of rsync as root.

- The patch is kept clean, minimal, and well understood.  

- The `update` script is intended to be run by cron for automatically 
  applying new upstream rsync versions. In contrast, no automation 
  is recommended for updating rsync-super patch.


## Miscelaneous

The super.c and super.h files have no license.