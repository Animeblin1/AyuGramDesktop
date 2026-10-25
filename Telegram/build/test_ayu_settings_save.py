from pathlib import Path
import unittest


class AyuSettingsSaveTest(unittest.TestCase):
    def test_save_uses_checked_atomic_replacement(self):
        source = (Path(__file__).resolve().parents[1]
                  / "SourceFiles/ayu/ayu_settings.cpp").read_text(encoding="utf-8")
        save = source.split("void AyuSettings::save() {", 1)[1].split(
            "void AyuSettings::reset()", 1)[0]
        self.assertIn("QSaveFile", save)
        self.assertNotIn("std::ofstream", save)
        self.assertNotIn("setDirectWriteFallback", save)
        self.assertLess(save.index("p.dump(4)"), save.index("file.open("))
        self.assertRegex(save, r"!file\.open\(QIODevice::WriteOnly\)\s*"
                         r"\|\| file\.write\(data\) != data\.size\(\)\s*"
                         r"\|\| !file\.commit\(\)")


if __name__ == "__main__":
    unittest.main()
