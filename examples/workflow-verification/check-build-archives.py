"""Archive only trusted synthetic artifacts, then move/extract/compare them."""
from pathlib import Path
import hashlib
import shutil
import tarfile
import zipfile

source = Path('staged files')
destination = Path('build archive destination')
destination.mkdir()

def contents(directory):
    return {
        file.relative_to(directory).as_posix(): hashlib.sha256(file.read_bytes()).hexdigest()
        for file in directory.rglob('*') if file.is_file()
    }

expected = contents(source)
assert len(expected) == 4
with zipfile.ZipFile('built projects.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in source.rglob('*'):
        if file.is_file():
            archive.write(file)
with tarfile.open('built projects.tar.gz', 'w:gz') as archive:
    archive.add(source)
for extension in ('zip', 'tar.gz'):
    original = Path(f'built projects.{extension}')
    digest = hashlib.sha256(original.read_bytes()).hexdigest()
    moved = Path(shutil.move(str(original), str(destination / original.name)))
    assert not original.exists()
    assert hashlib.sha256(moved.read_bytes()).hexdigest() == digest
    extracted = Path(f'build extracted {extension}')
    extracted.mkdir()
    if extension == 'zip':
        with zipfile.ZipFile(moved) as archive:
            archive.extractall(extracted)
    else:
        with tarfile.open(moved) as archive:
            # Debian's Python 3.11 predates extraction filters. This archive was
            # created above; still reject links and paths outside the test folder.
            for member in archive.getmembers():
                assert member.isfile() or member.isdir()
                assert (extracted / member.name).resolve().is_relative_to(extracted.resolve())
            archive.extractall(extracted)
    assert contents(extracted / source) == expected
print('Built Node/PHP ZIP/tar.gz bundles moved, extracted and verified')
