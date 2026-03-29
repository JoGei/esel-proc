#!/usr/bin/env bash
########################################################################################################################
# Package dependencies
get_apt_deps() {
  general_apt_dep="cmake build-essential git"
  verilator_apt_dep="verilator help2man autoconf automake autotools-dev bison curl flex g++ xz-utils wget libfl-dev ccache python3 python3-virtualenv python3-dev perl"
  rvgnu_apt_dep="autoconf automake autotools-dev curl git libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool bc zlib1g-dev"
  spike_apt_dep="libboost-dev wget tar device-tree-compiler libboost-regex-dev libboost-system-dev"
  fusesoc_apt_dep="python3-venv python3-pip"
  echo "${general_apt_dep} ${verilator_apt_dep} ${rvgnu_apt_dep} ${spike_apt_dep} ${fusesoc_apt_dep}"
}
setup_apt() {
  apt update
  apt install --no-install-recommends -y $(get_apt_deps)
}
setup_venv() {
  venv_dir="$1"
  workspace_dir="$2"
  req_file_list="${workspace_dir}/utils/python/requirements.txt"
  python3 -m venv "${venv_dir}"
  . .venv/bin/activate
  for req_file in ${req_file_list}; do
    echo "[venv] install from requirements file: ${req_file}"
    pip install -r "${req_file}"
  done
  fusesoc --version
}

########################################################################################################################
# Verilator
fetch_verilator() {
  _home_=${PWD}
  src_dir="$1"
  #build_dir="$2"
  install_dir="$3"
  version="${4}"

  verilator_tag="v${version}"
  verilator_url="https://github.com/verilator/verilator.git"

  echo "[fetch] verilator"
  git clone "${verilator_url}" "${src_dir}"
  cd "${src_dir}"
  git checkout "${verilator_tag}"
  rc=$?
  cd "${_home_}"
  return $rc
}
configure_verilator() {
  _home_=$PWD
  src_dir="$1"
  #build_dir="$2"
  install_dir="$3"
  version="${4}"

  echo "[configure] verilator install to [${install_dir}] from source [${src_dir}]"
  cd "${src_dir}"
  autoconf
  ./configure --prefix "${install_dir}"
  rc=$?
  echo "[configure] verilator done."
  cd "${_home_}"
  return $rc
}
build_verilator() {
  src_dir="$1"
  install_dir="$3"
  version="${4}"

  echo "[build] verilator"
  make -C "${src_dir}" -j $(nproc)
  rc =$?
  echo "[build] verilator done."
  return $rc
}
install_verilator() {
  src_dir="$1"

  echo "[install] verilator"
  make -C "${src_dir}" -j $(nproc) install
  rc=$?
  echo "[install] verilator done."
  return $rc
}
cleanup_verilator() {
  src_dir="$1"
  install_dir="$3"

  echo "[clean-up] verilator"
  rm -rf "${src_dir}"
  rc=$?
  echo "[clean-up] done."
  return $rc
}
setup_verilator() {
  src_dir="$1"
  build_dir="$2"
  install_dir="$3"
  version="${4}"
  if [ ! -f "${install_dir}/bin/verilator" ]; then
    fetch_verilator     "${src_dir}" "${build_dir}" "${install_dir}" "${version}" && \
    configure_verilator "${src_dir}" "${build_dir}" "${install_dir}" "${version}" && \
    build_verilator     "${src_dir}" "${build_dir}" "${install_dir}" "${version}" && \
    install_verilator   "${src_dir}" "${build_dir}" "${install_dir}" "${version}" && \
    cleanup_verilator   "${src_dir}" "${build_dir}" "${install_dir}" "${version}"
  fi
  ls -l "${install_dir}/bin" || true
  head -n 1 "${install_dir}/bin/verilator" || true
  "${install_dir}"/bin/verilator --version
}
# end: verilator ##############################################################
# rvgnu #######################################################################
fetch_rvgnu() {
  rvgnu_dir="$1"

  echo "[fetch] rvgnu ..."
  _home_=${PWD}
  url="https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v13.2.0-2/xpack-riscv-none-elf-gcc-13.2.0-2-linux-x64.tar.gz"
  target_name="xpack-riscv-none-elf-gcc-13.2.0-2"

  echo "[fetch] risc-v gnu tools"
  wget "${url}" -q --output-document="${target_name}-linux-x64.tar.gz"
  tar xf "${target_name}-linux-x64.tar.gz"
  mv "${target_name}" "${rvgnu_dir}"
  rm "${target_name}-linux-x64.tar.gz"

  #spike_ref="88edb8b81383bf282949be30476c9e4d5459cec4"
  #spike_url="https://github.com/riscv-software-src/riscv-isa-sim"
  #git clone ${spike_url}.git /tmp/spike-src
  #cd /tmp/spike-src
  #git checkout ${spike_ref}
  #cd ${_home_}

  #pk_ref="9c61d29846d8521d9487a57739330f9682d5b542"
  #pk_url="https://github.com/riscv-software-src/riscv-pk"
  #git clone ${pk_url}.git /tmp/pk-src
  #cd /tmp/pk-src
  #git checkout ${pk_ref}

  echo "[fetch] rvgnu done."
  cd ${_home_}
}
configure_rvgnu() {
  rvgnu_dir="$1"

  echo "[configure] rvgnu"
  _home_=${PWD}
  __PRE_RISCV__="${RISCV}"
  __PRE_PATH__="${PATH}"
  export RISCV="${rvgnu_dir}"
  export PATH="${RISCV}/bin:${PATH}"
  
  #_march_="rv32i_zicsr_zifencei"
  #mkdir -p /tmp/spike-src/build && cd /tmp/spike-src/build
  #../configure --prefix=${RISCV}
  #mkdir -p /tmp/pk-src/build && cd /tmp/pk-src/build
  #../configure --prefix=$RISCV --host=riscv-none-elf --with-arch="${_march_}"
  #make -j $(nproc)
  #make install
  #rc=$?
  export RISCV="${__PRE_RISCV__}"
  export PATH="${__PRE_PATH__}"
  cd ${_home_}
  echo "[configure] rvgnu done."
  return $rc
}
build_rvgnu() {
  echo "[build] rvgnu"
  make -C /tmp/pk-src/build -j $(nproc)
  make -C /tmp/spike-src/build -j $(nproc)
  echo "[build] rvgnu done."
}
install_rvgnu() {
  echo "[install] rvgnu"
  make -C /tmp/pk-src/build  install
  make -C /tmp/spike-src/build  install
  echo "[install] rvgnu done."
}
cleanup_rvgnu() {
  echo "[cleanup] rvgnu"
  rm -rf "/tmp/spike-src/" "/tmp/pk-src/"
  echo "[cleanup] rvgnu done."
}
setup_rvgnu() {
  rvgnu_dir=$1
  if [ ! -f "${rvgnu_dir}/bin/riscv-none-elf-gcc" ]; then
    fetch_rvgnu "${rvgnu_dir}" && \
    configure_rvgnu "${rvgnu_dir}" #&& \
    #build_rvgnu && \
    #install_rvgnu && \
    #cleanup_rvgnu 
  fi
  ls -l "${rvgnu_dir}/bin" || true
  head -n 1 "${rvgnu_dir}/bin/riscv-none-elf-gcc" || true
  "${rvgnu_dir}/bin/riscv-none-elf-gcc" --version
}
# end: rvgnu ##################################################################
setup() {
  _what_=$1
  echo "[setup] arg1: $1"
  echo "[setup] arg2: $2"
  echo "[setup] arg3: $3"
  echo "[setup] arg4: $4"
  echo "[setup] arg5: $5"
  echo "[setup] arg6: $6"
  setup_${_what_} "$2" "$3" "$4" "$5" "$6"
}
