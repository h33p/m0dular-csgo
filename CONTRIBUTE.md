# Contributing

**Contributions are welcome.** Before writing and submitting your awesome code, please take this document in consideration.

## Guidelines

We want to keep the code of the project consistent and the quality of it high, compile times low and portability high. The code guidelines are split into two categories (consistency and performance). Consistency guidelines are strict and non-debatable.

##### Consistency:
- Tabs are used for indentation.
- Files have to have correct line endings. Have autocrlf enabled.
- All variables are named using lowerCamelCase, constants with UPPERCASE_CHARACTERS. All functions and classes are named using UpperCamelCase.
- Pointer and reference signs go on the left side, right next to the type name.
- The curly brackets go on the new line only in function definitions, everywhere else they are to be placed on the same line as the (if/for/while) statement.
- Try keeping an empty line on a file end.
- As of any unmentioned details, be sure to follow the rest of the existing code.

##### Performance:
- Avoid using magic (in-code) statics. They increase the complexity of resulting code. One notable exception are the original functions of hooks. The mechanism provides a perfectly timed place to initialize the value of the original function, although even this might be reworked.
- Avoid including unnecessary headers inside the header file. Do that in the source file. This increases compile times, they are long enough as they stand. If some structures/classes are needed in the function arguments try just declaring the said type.
- The use of virtual functions most of the time is not welcome. There may be cases, however, where added code complexity is not worth some performance penalty. A good rule of thumb is to ask yourself: could I possibly determine here which function will be called here?
- Avoid megaclasses. The code is focused around data oriented design. That means code and objects are built around the data, not the other way around. When working with multiple entities, use structures of arrays, instead of arrays of structures. This allows for less cache misses and easier parallelism.

## Merging

The code has to be submitted for review to one of the maintainers. The maintainer might request some changes to the code as a goal to have the best possible user experience. To submit a pull request the contributor must follow the [Developer Certificate of Origin](https://developercertificate.org/) (DCO) and allow to license the code using the license of the project. Leave a `Signed-off-by:` footer at each of the commits (-s flag) or cryptographically sign them using the -S flag and their GPG key. Alternatively, sign off in the pull request message.

##### The DCO:
```
Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```
