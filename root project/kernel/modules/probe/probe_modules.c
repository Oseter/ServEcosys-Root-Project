#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <crypto/hash.h>
#include <linux/dmi.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/string.h>

#define SERVECOSYS_PROBE_VERSION "0.1.0"
#define FINGERPRINT_SIZE 32

static u8 cached_fingerprint[FINGERPRINT_SIZE];
static bool fingerprint_cached = false;

static int sha256_hash(const u8 *data, size_t data_len, u8 *out_hash, size_t hash_size)
{
    struct crypto_shash *tfm;
    struct shash_desc *desc;
    int ret;

    if (hash_size < FINGERPRINT_SIZE)
        return -EINVAL;

    tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    desc = kmalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
    if (!desc) {
        crypto_free_shash(tfm);
        return -ENOMEM;
    }

    desc->tfm = tfm;
    ret = crypto_shash_init(desc);
    if (!ret)
        ret = crypto_shash_update(desc, data, data_len);
    if (!ret)
        ret = crypto_shash_final(desc, out_hash);

    kfree(desc);
    crypto_free_shash(tfm);
    return ret;
}

static int collect_smbios_data(u8 *buffer, size_t buf_size)
{
    size_t offset = 0;
    const char *dmi_strings[] = {
        dmi_get_system_info(DMI_SYS_VENDOR),
        dmi_get_system_info(DMI_PRODUCT_NAME),
        dmi_get_system_info(DMI_PRODUCT_VERSION),
        dmi_get_system_info(DMI_BIOS_VERSION),
        dmi_get_system_info(DMI_BOARD_NAME),
        dmi_get_system_info(DMI_BOARD_SERIAL),
    };
    int i;

    for (i = 0; i < ARRAY_SIZE(dmi_strings); i++) {
        if (dmi_strings[i] && offset < buf_size) {
            size_t len = strlen(dmi_strings[i]) + 1;
            if (offset + len > buf_size)
                len = buf_size - offset;
            memcpy(buffer + offset, dmi_strings[i], len);
            offset += len;
        }
    }

    return offset;
}

static void collect_pci_ids(u8 *buffer, size_t *offset, size_t buf_size)
{
    struct pci_dev *pdev = NULL;

    for_each_pci_dev(pdev) {
        u16 ids[4] = {
            pdev->vendor, pdev->device,
            pdev->subsystem_vendor, pdev->subsystem_device
        };
        size_t ids_size = sizeof(ids);

        if (*offset + ids_size > buf_size)
            break;

        memcpy(buffer + *offset, ids, ids_size);
        *offset += ids_size;
    }

    pci_dev_put(pdev);
}

int servecosys_generate_fingerprint(u8 *buffer, size_t size)
{
    u8 *data_buf;
    size_t data_size = 4096;
    int ret;

    if (size < FINGERPRINT_SIZE)
        return -EINVAL;

    if (fingerprint_cached) {
        memcpy(buffer, cached_fingerprint, FINGERPRINT_SIZE);
        return 0;
    }

    data_buf = kmalloc(data_size, GFP_KERNEL);
    if (!data_buf)
        return -ENOMEM;

    memset(data_buf, 0, data_size);

    size_t offset = collect_smbios_data(data_buf, data_size);
    collect_pci_ids(data_buf, &offset, data_size);

    if (offset < data_size) {
        u32 timestamp = (u32)(ktime_get_real_fast_ns() & 0xFFFFFFFF);
        memcpy(data_buf + offset, &timestamp, sizeof(timestamp));
        offset += sizeof(timestamp);
    }

    ret = sha256_hash(data_buf, offset, buffer, size);

    if (ret == 0) {
        memcpy(cached_fingerprint, buffer, FINGERPRINT_SIZE);
        fingerprint_cached = true;
        pr_info("ServEcosys Probe: Hardware fingerprint generated (%zu bytes hashed)\n", offset);
    }

    kfree(data_buf);
    return ret;
}
EXPORT_SYMBOL(servecosys_generate_fingerprint);

static int __init servecosys_probe_init(void)
{
    pr_info("ServEcosys Probe Module v%s (SHA256 hardware fingerprint)\n", SERVECOSYS_PROBE_VERSION);
    return 0;
}

static void __exit servecosys_probe_exit(void)
{
    pr_info("ServEcosys Probe Module unloaded\n");
}

module_init(servecosys_probe_init);
module_exit(servecosys_probe_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ServEcosys Project");
MODULE_DESCRIPTION("ServEcosys Hardware Fingerprint Probe (SHA256 + DMI + PCI)");
MODULE_VERSION(SERVECOSYS_PROBE_VERSION);
