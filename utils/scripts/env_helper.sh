#!/usr/bin/env bash
# lint #########################################################################
source_env_lint() {
  env_file="$1"
  echo "[source_env_lint] from file ${env_file}"
  while IFS='=' read -r key value; do
    [[ -z "$key" || "$key" =~ ^# ]] && continue
    key=${key#ENV_}
    value_expanded=$(eval echo "$value")
    export "$key=$value_expanded"
    echo "$key=$value_expanded"
  done < ${env_file}

  export PATH="${VERILATOR_INSTALL_DIR}/bin:${PATH}"
  export PATH="${RVGNU_INSTALL_DIR}/bin:${PATH}"
  export PATH="${SPIKE_INSTALL_DIR}/bin:${PATH}"
  echo "[source_env_lint] done."
}
# end: lint environment helper #################################################
# sim ##########################################################################
source_env_sim() {
  env_file="$1"
  source_env_lint "${env_file}"
}
# end: sim environment helper ##################################################
source_env() {
  _what_="$1"
  env_file="$2"
  venv_dir="$3"
  echo "[source env] from file ${env_file}, activate venv from ${venv_dir}"
  . "${venv_dir}/bin/activate"
  source_env_${_what_} "${env_file}"
}

source_env "sim" "$1" "$2"