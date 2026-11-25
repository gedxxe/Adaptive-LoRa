import csv
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

from secure_provision import config_blob, provision


def blobs(path):
    with path.open(encoding="utf-8", newline="") as file:
        return {r["key"]: bytes.fromhex(r["value"]) for r in csv.DictReader(file) if r["type"] == "data"}


class SecureProvisionTests(unittest.TestCase):
    def test_matching_credentials_bound_to_role_and_revocation(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "offline"
            self.assertEqual(2, provision(output, 9, 2, 3, {3}, 3000))
            gateway = blobs(output / "gateway-2.csv")
            first = blobs(output / "client-1.csv")
            last = blobs(output / "client-4.csv")
            self.assertFalse((output / "client-3.csv").exists())
            self.assertEqual(config_blob(2, 9, 4, 2, 3000), last["device"])
            record = gateway["v7"]
            self.assertEqual(28 + 32 + 3 * 16, len(record))
            public = ec.derive_private_key(int.from_bytes(record[28:60], "big"), ec.SECP256R1()).public_key()
            self.assertEqual(public.public_bytes(Encoding.X962, PublicFormat.UncompressedPoint), first["v7"][28:93])
            self.assertEqual(record[12:28], first["v7"][12:28])
            self.assertEqual(record[60:76], first["v7"][-16:])
            self.assertEqual(bytes(16), record[76:92])
            self.assertEqual(record[92:108], last["v7"][-16:])
            with self.assertRaises(FileExistsError):
                provision(output, 9, 2, 3, set(), 3000)

    def test_invalid_inputs_fail_before_creation(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "offline"
            for count, revoked in ((129, set()), (1, {2}), (4, {6})):
                with self.assertRaises(ValueError):
                    provision(path, 1, 1, count, revoked, 3000)
                self.assertFalse(path.exists())

    def test_sdk_builds_nvs_image_with_full_registry_record(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "offline"
            provision(output, 1, 1, 128, set(range(3, 130)), 3000)
            for name in ("gateway-1", "client-2"):
                image = output / f"{name}.bin"
                subprocess.run([sys.executable, "-m", "esp_idf_nvs_partition_gen", "generate", str(output / f"{name}.csv"),
                                str(image), "0x5000"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                self.assertEqual(0x5000, image.stat().st_size)
