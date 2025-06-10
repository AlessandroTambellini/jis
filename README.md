<p align="center">
  <img src="jis-logo.png" alt="Jis logo" width="400" style="border-radius: 12px;">
</p>

# Jis
The Jis Programming Language.

> [!WARNING]
> Under development.

The programs inside the `examples` folder are commented and cover all the syntax of the language.

## Design of the language

### Error Reporting
- As soon as a single parsing error is encountered, the program exits.  
I made this choice because, often, subsequent errors are originated from few original ones,
and, at the same time, I often debug one error at a time.  
This isn't true for syntax errors: these are reported all toghether because one can't be influenced by the
previous one.  
- There aren't warnings. It's an error or it's valid.

### VM
I'm resisting adding a vm. It's an experiment.
