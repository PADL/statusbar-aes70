# statusbar-aes70 documentation

The `padl-aes70` package is the OCP.1 encoding of AES70 (AES70-3) and the AES70 data types a
program exchanging OCP.1 messages over a channel of its own needs. Each module is documented in
its own overview file; pick a module for an orientation, then read its headers under
`padl/<module>/`.

## Module overviews

| Module | Purpose                                                                                         | Link                             |
|--------|-------------------------------------------------------------------------------------------------|----------------------------------|
| ocp1   | AES70-3 values (`Encoder`, `Decoder`), PDU framing, parsing and serializing, `status_for()`.     | [OCP1_MODULE.md](OCP1_MODULE.md) |
| oca    | AES70-2 data types of the connection management classes, and the AES70-21 AES67 adaptation data. | [OCA_MODULE.md](OCA_MODULE.md)   |
