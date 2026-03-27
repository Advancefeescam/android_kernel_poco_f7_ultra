load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load("//build/kernel/kleaf:kernel.bzl", "kernel_build","kernel_build_config")
load(":dtb_src.bzl","get_dtbo_list","get_dtb_list")

_O1_CONFIGS = {
    "xring_O1_dtb": {
        "soc_type": "O1",
        "srcs": "//xring-modules:xring_device_common_sources",
        "build_config": "//xring-configs/O1:build.config.O1_dtb.aarch64",
        "dtstree": "//xring-dts/O1:O1_dt",
    },
}
# -----------------------------------------
#            only build dtb
# -----------------------------------------
def build_dtb():
    for name, config in _FRANKLIN_CONFIGS.items():
        build_config_fragments =[
            config["build_config"],
        ]
        native.genrule(
            name = "{}_set_dts_flag".format(name),
            outs = ["build.config.{}.set_platfom".format(name)],
            cmd_bash = """
                DTC_FLAGS=$${DTC_FLAGS:-''}
                echo "DTC_FLAGS='$$DTC_FLAGS'" >> "$@"
            """
        )
        kernel_build_config(
            name = "{}_build_config".format(name),
            srcs = [
            "{}_set_dts_flag".format(name),
            ] + [fragment for fragment in build_config_fragments],
        )
        kernel_build(
            name = name,
            srcs = [config["srcs"]],
            outs = get_dtbo_list(config["soc_type"]) + get_dtb_list(config["soc_type"]),
            base_kernel = "//common:kernel_aarch64",
            build_config = "{}_build_config".format(name),
            dtstree = config["dtstree"],
            make_goals = [
                "dtbs",
            ],
        )
        copy_to_dist_dir(
            name = name + "_dist",
            data = [
                name,
            ],
            dist_dir = "out/android15-6.6/dist",
            flat = True,
            log = "info",
        )