This repository holds the BlackBerry 10 port of Python-3.2.2.

Our intent with this repository is:
  1. Fulfill any license requirements by publishing any of our source code changes
  2. To enable the larger community to build their own binaries which will run on BlackBerry 10 / PlayBook devices
  3. To prepare and formalize our patch-set, in preparation for a push to the upstream Python.org repository.  Community assistance with this effort will be greatly appreciated, as internal commitments have most RIM employees tied up right now :)

Build notes
  1. Install BlackBerry 10 NDK tools and make sure environment variables are set up
  2. Seems we need a case-sensitive build environment (partition accordingly on OSX, or create a DMG to build in)
  3. May need to install Mercurial (Hg)
  4. May need an existing host-native python installation