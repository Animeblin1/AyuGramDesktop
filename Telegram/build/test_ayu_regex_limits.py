from pathlib import Path
import unittest


class AyuRegexLimitsTest(unittest.TestCase):
    def test_matching_is_bounded_and_errors_do_not_reverse_into_matches(self):
        source = (Path(__file__).resolve().parents[1]
                  / "SourceFiles/ayu/features/filters/filters_controller.cpp"
                  ).read_text(encoding="utf-8")
        match = source.split("const auto matches =", 1)[1].split(
            "const auto reversed =", 1)[0]
        self.assertIn("matcher->setTimeLimit(100, status)", match)
        self.assertIn("matcher->setStackLimit(1024 * 1024, status)", match)
        self.assertNotIn("matcher->find()", match)
        self.assertRegex(match, r"matcher->find\(status\);\s*"
                         r"if \(U_FAILURE\(status\)\) \{[^}]*return false;")
        self.assertLess(match.index("setTimeLimit"), match.index("find(status)"))
        self.assertLess(match.index("setStackLimit"), match.index("find(status)"))


if __name__ == "__main__":
    unittest.main()
