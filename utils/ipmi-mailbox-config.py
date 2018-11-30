#!/usr/bin/env python3

import argparse
import subprocess

def send_block_read_command(hostname, username, password, index, dry_run):

    if hostname is None and dry_run:
        hostname = "<hostname>"

    cmd = "ipmitool -I lanplus -H " + hostname
    if username is not None:
        cmd = cmd + " -U " + username
    if password is not None:
        cmd = cmd + " -P " + password

    # chassis netfn, get-sys-boot-options, parameter 7, set-sel, block-sel
    cmd = cmd + " raw 0x00 0x09 0x07 " + hex(index) + " 0x00 "

    print(cmd)
    if not dry_run:
        rc = subprocess.call(cmd, shell=True)
        if rc != 0:
            print("Command returned error: {}".format(rc))

def send_block_command(hostname, username, password, block, index, dry_run):

    if hostname is None and dry_run:
        hostname = "<hostname>"

    cmd = "ipmitool -I lanplus -H " + hostname
    if username is not None:
        cmd = cmd + " -U " + username
    if password is not None:
        cmd = cmd + " -P " + password

    # chassis netfn, set-sys-boot-options, parameter 7, set-sel, block-sel
    cmd = cmd + " raw 0x00 0x08 0x07 " + hex(index) + " " + block

    print(cmd)
    if not dry_run:
        rc = subprocess.call(cmd, shell=True)
        if rc != 0:
            print("Command returned error: {}".format(rc))

def construct_buffer(config, max_blocks):

    # Add IBM's IANA prefix
    raw = bytes([0x02, 0x00, 0x00]) + config.encode('ascii')

    n_blocks = int(len(raw) / 16)
    if len(raw) % 16 != 0:
        n_blocks += 1

    if n_blocks > 255:
        print("ERROR: buffer would require {} blocks".format(n_blocks) +
                " which is more than hypothetically possible (255)")
        return None

    if n_blocks > max_blocks:
        print("ERROR: buffer would require {} blocks".format(n_blocks) +
                " which is more than max_blocks ({})".format(max_blocks))
        return None

    if n_blocks > 5:
        print("Warning: buffer would require {} blocks".format(n_blocks) +
                "which is more than some BMCs support")

    blocks = []
    rem = len(raw)
    for i in range(n_blocks):
        block = ""
        if rem >= 16:
            last = 16
        else:
            last = rem

        for j in range(16):
            if j < last:
                block += "{:#02x} ".format(raw[i * 16 + j])
            else:
                # Pad out to 16 bytes
                block += "0x00 "


        blocks.append(block)
        rem -= last

    if n_blocks < max_blocks:
        for i in range(max_blocks - n_blocks):
            blocks.append("0x00 0x00 0x00 0x00 " +
                          "0x00 0x00 0x00 0x00 " +
                          "0x00 0x00 0x00 0x00 " +
                          "0x00 0x00 0x00 0x00")

    return blocks

def construct_empty_buffer(max_blocks):

    blocks = []
    for i in range(max_blocks):
        blocks.append("0x00 0x00 0x00 0x00 " +
                      "0x00 0x00 0x00 0x00 " +
                      "0x00 0x00 0x00 0x00 " +
                      "0x00 0x00 0x00 0x00")

    return blocks

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("-b", "--bmc-hostname")
    parser.add_argument("-u", "--username")
    parser.add_argument("-p", "--password")
    parser.add_argument("-n", "--dry-run", action="store_true")
    parser.add_argument("-c", "--config")
    parser.add_argument("-x", "--clear", action="store_true")
    parser.add_argument("-d", "--dump", action="store_true")
    parser.add_argument("-m", "--max-blocks")

    args = parser.parse_args()

    if not args.dry_run and args.bmc_hostname is None:
        print("No hostname specified!")
        return -1

    if args.config and args.clear:
        print("Can't specify --config and --clear together")
        return -1

    if args.max_blocks:
        n_blocks = int(args.max_blocks)
    else:
        n_blocks = 16


    if args.config or args.clear:
        if args.config:
            blocks = construct_buffer(args.config, int(args.max_blocks))
        if args.clear:
            blocks = construct_empty_buffer(int(args.max_blocks))
        if blocks is None:
            print("Failed to construct buffer")
            return -1

        print("{} blocks to send".format(len(blocks)))
        print("---------------------------------------")
        for i in range(len(blocks)):
            try:
                send_block_command(args.bmc_hostname, args.username, args.password,
                        blocks[i], i, args.dry_run)
            except Exception as e:
                print(e)
                print("Error sending block {}".format(i))
                return -1
            i += 1

    if args.dump:
        print("\nReading {} blocks".format(n_blocks))
        print("---------------------------------------")
        for i in range(n_blocks):
            send_block_read_command(args.bmc_hostname, args.username,
                    args.password, i, args.dry_run)

if __name__ == "__main__":
    main()
