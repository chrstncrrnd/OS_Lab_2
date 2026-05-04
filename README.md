# OS Lab 2: `uc3mshell`
A unix shell written using syscalls exclusively for process management and file reading/writing. 

## Usage: 
```./uc3mshell <input_file>```

# Features:
- Reads commands from input file. 
- Unlimited `stdin` and `stdout` redirections using pipes. 
For example: `cat example.py | wc -l`.
- File input/output/error redirection: 
    * `curl https://google.com !> err.log > page.html`
    * `cat file.txt | shuf | head -n 1 > random_line.txt`
    * `shuf < file.txt | head -n 1 > random_line.txt`
- Finite automata for tokenizer and parser. This allows for parsing of lines such as: `echo"hello"|wl-copy` (i.e. no spaces).
- Builtin commands:
    * Exit: `exit <exit_value>`
    * [Mycalc](https://github.com/chrstncrrnd/OS_Lab_1): `mycalc <operand1> <operator> <operand2>`
- Copy command: `mycp <from> <to>`