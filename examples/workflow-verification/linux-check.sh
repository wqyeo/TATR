set -eu
cd /work/builds
/runner/tatr --version
node --version
npm --version
php --version
composer --version
cmake --version
test ! -e logs/build-actions.log
/runner/tatr --file builds.toml list > list.txt
/runner/tatr --file builds.toml plan archive-builds > plan.txt
test ! -e logs/build-actions.log
/runner/tatr --file builds.toml run archive-builds > build-stdout.log 2> build-console.log
grep -q 'Node/PHP default/custom outputs and copy/move verified' build-stdout.log
grep -q 'Built Node/PHP ZIP/tar.gz bundles moved, extracted and verified' build-stdout.log
grep -q 'event=run_finished.*outcome=success' logs/build-actions.log
grep '^time=' build-console.log > build-console-actions.log
cmp build-console-actions.log logs/build-actions.log
if grep -q 'Node build completed:' logs/build-actions.log; then exit 1; fi
set +e
/runner/tatr --file builds.toml run must-not-run > failure-stdout.log 2> failure-console.log
code=$?
set -e
test "$code" -eq 1
test ! -e build-failure-marker
grep -q 'event=task_finished.*task="build-fail".*outcome=child_failure' failure-console.log
if grep -q 'event=task_started task="must-not-run"' failure-console.log; then exit 1; fi
cd /work/linux-files
/runner/tatr --file files.toml run files > files-stdout.log 2> files-console.log
grep -q 'ZIP/tar.gz create, move, extract and content comparisons passed' files-stdout.log
cmp files-console.log logs/file-actions.log
echo 'Linux build, file, logging and failure checks passed'
