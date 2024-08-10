set -x
../compress endl.elf endl.bpe > compress.log
../expand   endl.bpe endl.new > expand.log
diff endl.elf endl.new
