<?php
require __DIR__ . '/vendor/autoload.php';
$output = $argv[1] ?? 'dist';
if (getenv('FIXTURE_MESSAGE') !== 'built through TATR') {
    throw new RuntimeException('Expected dotenv fixture value');
}
if (!is_dir($output) && !mkdir($output, 0777, true)) {
    throw new RuntimeException('Cannot create output');
}
if (file_put_contents($output . '/index.txt', fixture_message(getenv('FIXTURE_MESSAGE'))) === false) {
    throw new RuntimeException('Cannot write artifact');
}
echo "PHP application packaging completed: {$output}\n";
