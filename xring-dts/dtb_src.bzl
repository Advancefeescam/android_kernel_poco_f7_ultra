def get_dtbo_list(
    soc_type = None,
):
    if soc_type == None:
        return []

    return DTB_LIST[soc_type]["dtbo_list"]

def get_dtb_list(
    soc_type = None,
):
    if soc_type == None:
        return []

    return DTB_LIST[soc_type]["dtb_list"]

O1_DTBOS = [
    "overlay/udp/udp_phone/franklin1_udp_phone_overlay.dtbo",
    "overlay/udp/udp_pad/franklin1_udp_pad_overlay.dtbo",
    "overlay/product/phone/phone_o2s/franklin1_o2s_phone_overlay.dtbo",
    "overlay/product/pad/pad_o80/franklin1_o80_pad_overlay.dtbo",
    "overlay/product/pad/pad_o81a/franklin1_o81a_pad_overlay.dtbo",
]

O1_DTBS = [
    "soc/xring_o1_basic.dtb",
]

DTB_LIST = {
    "O1": {
        "dtb_list": O1_DTBS,
        "dtbo_list": O1_DTBOS,
    },
}
