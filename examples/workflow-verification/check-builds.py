"""Assert real fixture outputs, then copy/move them into staging."""
from pathlib import Path
import shutil

stage = Path('staged files')
stage.mkdir()
for kind in ('node', 'php'):
    default = Path(kind) / 'dist' / 'index.txt'
    custom = Path(f'custom {kind} output') / 'index.txt'
    expected = f'{kind.capitalize() if kind == "node" else "PHP"}: built through TATR\n'
    assert default.read_text() == expected
    assert custom.read_text() == expected
    shutil.copytree(default.parent, stage / f'{kind} default')
    shutil.move(str(custom.parent), str(stage / f'{kind} custom'))
    assert not custom.exists()
    assert (stage / f'{kind} custom' / 'index.txt').read_text() == expected
print('Node/PHP default/custom outputs and copy/move verified')
