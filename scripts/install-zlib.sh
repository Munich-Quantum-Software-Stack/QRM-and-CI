ZLIB_INSTALL_PREFIX=/usr/local/
${CC:=gcc}

function remove_temp_installs {
  if [ -n "$PKG_UNINSTALL" ]; then
    echo "Uninstalling packages used for bootstrapping: $PKG_UNINSTALL"
    if [ -x "$(command -v apt-get)" ]; then
      apt-get remove -y $PKG_UNINSTALL
      apt-get autoremove -y --purge
    elif [ -x "$(command -v dnf)" ]; then
      dnf remove -y $PKG_UNINSTALL
      dnf clean all
    else
      echo "No package manager configured for clean up." >&2
    fi
    unset PKG_UNINSTALL
  fi
}

# [Zlib] Needed to build LLVM with zlib support (used by linker)
if [ -n "$ZLIB_INSTALL_PREFIX" ] && [ -z "$(echo $exclude_prereq | grep zlib)" ]; then
  #if [ ! -f "$ZLIB_INSTALL_PREFIX/lib/libz.a" ]; then
    echo "Installing libz..."
    temp_install_if_command_unknown wget wget
    temp_install_if_command_unknown make make
    temp_install_if_command_unknown automake automake
    temp_install_if_command_unknown libtool libtool

    wget https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz
    tar -xzvf zlib-1.3.tar.gz && cd zlib-1.3
    CC="$CC" CFLAGS="-fPIC" \
    ./configure --prefix="$ZLIB_INSTALL_PREFIX" --static
    make CC="$CC" && make install
    cd contrib/minizip
    autoreconf --install
    CC="$CC" CFLAGS="-fPIC" \
    ./configure --prefix="$ZLIB_INSTALL_PREFIX" --disable-shared
    make CC="$CC" && make install
    cd ../../.. && rm -rf zlib-1.3.tar.gz zlib-1.3
    remove_temp_installs
  #else
  #  echo "libz already installed in $ZLIB_INSTALL_PREFIX."
  #fi
fi
