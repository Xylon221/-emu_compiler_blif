"""Check model summaries, token equality, parser failures and repeated runs."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
def run(*args):
    return subprocess.run([str(a) for a in args], text=True, capture_output=True, check=True).stdout
with tempfile.TemporaryDirectory() as tmp:
    fixture = Path(tmp) / 'fixture.blif'
    for gates, latches in [(0, 0), (1, 1), (1000, 32)]:
        run(sys.executable, root / 'scripts/gen_blif.py', '--gates', gates,
            '--latches', latches, '--out', fixture)
        summaries = []
        for variant in ['strdup', 'intern']:
            output = run(root / f'build/blif_bench_{variant}', fixture, 3)
            summary = re.search(r'^model=.*$', output, re.M).group(0)
            assert f'gates={gates} latches={latches}' in summary, summary
            summaries.append(summary)
        assert summaries[0] == summaries[1], summaries
        output = run(root / 'build/handwritten_bench', fixture, 2)
        assert 'PASS' in output, output
        print(f'PASS: gates={gates} latches={latches}, model hash + token equality')
    for variant in ['strdup', 'intern']:
        binary = root / f'build/blif_bench_{variant}'
        assert subprocess.run([str(binary), str(fixture), '0'], capture_output=True).returncode == 2
        fixture.write_text('.model\n', encoding='utf-8')
        assert subprocess.run([str(binary), str(fixture)], capture_output=True).returncode != 0
    print('PASS: zero iterations and malformed input rejected')
