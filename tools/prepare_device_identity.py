"""Prepare a private NVS image from an EXISTING credential. Never contacts or flashes a device."""
import argparse
import contextlib
import csv
import io
import json
import os
from pathlib import Path
import re
import stat
import tempfile

PARTITION_SIZE = 0x6000


def load_identity(path):
    with path.open() as source:
        mode = os.fstat(source.fileno()).st_mode
        if stat.S_IMODE(mode) & 0o077:
            raise ValueError('Credential file must be private')
        raw = source.read(257)
    if len(raw.encode('utf-8')) > 256:
        raise ValueError('Credential too large')
    identity = json.loads(raw)
    if (not isinstance(identity, dict) or set(identity) != {'deviceId', 'token'}
            or not isinstance(identity['deviceId'], str)
            or not re.fullmatch(r'[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}', identity['deviceId'])
            or not isinstance(identity['token'], str)
            or not re.fullmatch(r'dev1_[a-f0-9]{64}', identity['token'])):
        raise ValueError('Invalid credential record')
    return json.dumps(identity, separators=(',', ':'))


def prepare(credential, output):
    record = load_identity(credential)
    # Restrict both the final artifact and any tool-created temporary plaintext files.
    if not output.parent.is_dir() or stat.S_IMODE(output.parent.stat().st_mode) & 0o077:
        raise ValueError('Output directory must already exist with mode 0700')
    from esp_idf_nvs_partition_gen import nvs_partition_gen
    old_mask = os.umask(0o077)
    try:
        with tempfile.TemporaryDirectory(prefix='.identity-', dir=output.parent) as temporary:
            temporary = Path(temporary)
            source = temporary / 'identity.csv'
            with source.open('x', newline='') as target:
                writer = csv.writer(target)
                writer.writerow(['key', 'type', 'encoding', 'value'])
                writer.writerow(['identity', 'namespace', '', ''])
                writer.writerow(['record', 'data', 'string', record])
            image = temporary / 'identity.bin'
            args = argparse.Namespace(input=str(source), output=str(image), size=hex(PARTITION_SIZE),
                                      version=2, outdir=str(temporary))
            # The upstream tool may include input in diagnostics. Never forward its output.
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                nvs_partition_gen.generate(args)
            data = image.read_bytes()
            if len(data) != PARTITION_SIZE:
                raise ValueError('Invalid image size')
            # Exclusive creation: never overwrite a prepared identity.
            with output.open('xb') as target:
                target.write(data)
    finally:
        os.umask(old_mask)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('credential', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        prepare(args.credential, args.output)
    except (Exception, SystemExit):
        print('Identity preparation failed; check private input, output and NVS tool installation.')
        return 1
    print('Private device-auth image prepared. No credential generated, replaced or flashed.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
