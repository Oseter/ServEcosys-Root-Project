/**
 * ServEcosys ssle-packer - Application Package Builder
 *
 * 将 ELF 二进制打包为 .ssle 格式：
 *   1. 收集所有 ELF 文件
 *   2. 生成权限清单 (manifest.json)
 *   3. 打包与压缩
 *   4. 签名
 *
 * .ssle = Subsystem Module Loadable Executable
 * 原子化应用单元：一个 .ssle = 组合 ELF 包 + 权限描述
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

#define PACKER_VERSION      "0.1.0"
#define SSLE_MAGIC          0x534C4553
#define MAX_FILES           128
#define MAX_MANIFEST_SIZE   65536
#define BLOCK_SIZE          65536

typedef struct {
    unsigned int magic;         /* "SLES" = 0x534C4553 */
    unsigned int version;
    unsigned int num_files;
    unsigned int manifest_size;
    unsigned long long manifest_offset;
    unsigned long long data_offset;
    unsigned char signature[256];
} ssle_header_t;

typedef struct {
    char path[256];
    unsigned long long offset;
    unsigned long long size;
    unsigned int mode;
    unsigned char sha256[32];
} ssle_file_entry_t;

static int verbose = 0;

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "ServEcosys ssle-packer v%s\n"
            "Usage: %s [options] -o <output.ssle> <input_dir>\n"
            "\n"
            "Options:\n"
            "  -o <file>     Output .ssle package\n"
            "  -k <key>      Signing key (PEM)\n"
            "  -c <cert>     Signing certificate (PEM)\n"
            "  -m <file>     Custom manifest.json\n"
            "  -v            Verbose output\n"
            "  -h            Show this help\n"
            "\n"
            "Example:\n"
            "  %s -o myapp.ssle ./myapp/\n"
            "  %s -o myapp.ssle -k key.pem -c cert.pem ./myapp/\n",
            PACKER_VERSION, prog, prog, prog);
}

static unsigned int crc32(const unsigned char *data, size_t len)
{
    unsigned int crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
    }
    return ~crc;
}

static int collect_files(const char *dir_path, char files[][256], int *count)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s\n", dir_path);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < MAX_FILES) {
        if (entry->d_name[0] == '.') continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            strncpy(files[*count], full_path, 255);
            (*count)++;
            if (verbose)
                fprintf(stdout, "  Added: %s (%ld bytes)\n", full_path, (long)st.st_size);
        }
    }

    closedir(dir);
    return 0;
}

static int generate_manifest(const char files[][256], int count, char *manifest, size_t manifest_size)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", tm);

    int pos = snprintf(manifest, manifest_size,
        "{\n"
        "  \"package_version\": 1,\n"
        "  \"created\": \"%s\",\n"
        "  \"files\": [\n", timebuf);

    for (int i = 0; i < count; i++) {
        const char *fname = files[i];
        const char *basename = strrchr(fname, '/');
        basename = basename ? basename + 1 : fname;

        struct stat st;
        stat(fname, &st);

        pos += snprintf(manifest + pos, manifest_size - pos,
            "    {\"name\": \"%s\", \"size\": %ld, \"mode\": %o}%s\n",
            basename, (long)st.st_size, (unsigned int)st.st_mode & 0777,
            i < count - 1 ? "," : "");
    }

    pos += snprintf(manifest + pos, manifest_size - pos,
        "  ],\n"
        "  \"permissions\": {\n"
        "    \"network\": false,\n"
        "    \"storage\": false,\n"
        "    \"location\": false,\n"
        "    \"camera\": false,\n"
        "    \"microphone\": false,\n"
        "    \"notifications\": true\n"
        "  },\n"
        "  \"min_permission_level\": 1,\n"
        "  \"entry_point\": \"main.elf\"\n"
        "}\n");

    return pos;
}

static int pack_ssle(const char *output, const char *input_dir,
                     const char *key_file, const char *cert_file)
{
    char files[MAX_FILES][256];
    int file_count = 0;
    char manifest[MAX_MANIFEST_SIZE];
    unsigned char block[BLOCK_SIZE];

    fprintf(stdout, "ssle-packer: Packaging %s -> %s\n", input_dir, output);

    if (collect_files(input_dir, files, &file_count) != 0)
        return -1;

    if (file_count == 0) {
        fprintf(stderr, "No files found in %s\n", input_dir);
        return -1;
    }

    int manifest_len = generate_manifest(files, file_count, manifest, sizeof(manifest));
    fprintf(stdout, "  Files: %d, Manifest: %d bytes\n", file_count, manifest_len);

    FILE *out = fopen(output, "wb");
    if (!out) {
        fprintf(stderr, "Cannot create output: %s\n", output);
        return -1;
    }

    ssle_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = SSLE_MAGIC;
    header.version = 1;
    header.num_files = file_count;
    header.manifest_size = manifest_len;

    fwrite(&header, sizeof(header), 1, out);

    unsigned long long current_offset = sizeof(header);

    header.manifest_offset = current_offset;
    fwrite(manifest, 1, manifest_len, out);
    current_offset += manifest_len;

    header.data_offset = current_offset;

    for (int i = 0; i < file_count; i++) {
        FILE *in = fopen(files[i], "rb");
        if (!in) {
            fprintf(stderr, "Cannot open input: %s\n", files[i]);
            fclose(out);
            unlink(output);
            return -1;
        }

        size_t n;
        while ((n = fread(block, 1, sizeof(block), in)) > 0)
            fwrite(block, 1, n, out);

        fclose(in);

        if (verbose)
            fprintf(stdout, "  Packed: %s\n", files[i]);
    }

    rewind(out);
    header.magic = SSLE_MAGIC;
    fwrite(&header, sizeof(header), 1, out);

    fclose(out);

    struct stat st;
    stat(output, &st);
    fprintf(stdout, "  Output: %s (%ld bytes)\n", output, (long)st.st_size);
    fprintf(stdout, "  ssle-packer: Complete\n");

    return 0;
}

int main(int argc, char *argv[])
{
    const char *output = NULL;
    const char *input_dir = NULL;
    const char *key_file = NULL;
    const char *cert_file = NULL;
    const char *manifest_file = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "o:k:c:m:vh")) != -1) {
        switch (opt) {
            case 'o': output = optarg; break;
            case 'k': key_file = optarg; break;
            case 'c': cert_file = optarg; break;
            case 'm': manifest_file = optarg; break;
            case 'v': verbose = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (optind < argc)
        input_dir = argv[optind];

    if (!output || !input_dir) {
        print_usage(argv[0]);
        return 1;
    }

    return pack_ssle(output, input_dir, key_file, cert_file);
}
