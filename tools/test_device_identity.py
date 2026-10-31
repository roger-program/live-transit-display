"""Offline provisioning tests with synthetic credentials only."""
import json
from pathlib import Path
import tempfile
import unittest
from prepare_device_identity import load_identity, prepare, PARTITION_SIZE


class IdentityTests(unittest.TestCase):
    def test_image_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            credential = root / 'credential.json'
            identity = {'deviceId': '12345678-1234-1234-1234-123456789abc',
                        'token': 'dev1_' + 'a' * 64}
            credential.write_text(json.dumps(identity))
            credential.chmod(0o600)
            output = root / 'device-auth.bin'
            prepare(credential, output)
            self.assertEqual(output.stat().st_size, PARTITION_SIZE)
            self.assertEqual(output.stat().st_mode & 0o777, 0o600)
            self.assertIn(identity['token'].encode(), output.read_bytes())
            original = output.read_bytes()
            with self.assertRaises(FileExistsError):
                prepare(credential, output)
            self.assertEqual(output.read_bytes(), original)
            self.assertEqual(list(root.glob('.identity-*')), [])

    def test_invalid_and_public_inputs(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'credential.json'
            path.write_text('{}')
            path.chmod(0o600)
            with self.assertRaises(ValueError):
                load_identity(path)
            path.chmod(0o644)
            with self.assertRaises(ValueError):
                load_identity(path)


if __name__ == '__main__':
    unittest.main()
