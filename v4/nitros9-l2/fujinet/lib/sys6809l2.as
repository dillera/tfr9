* sys6809l2.as -- NitrOS-9's lib/sys6809l2.as, but finding the definitions
* on the include path, so that fujinet/defs/drivewire.d (with the FujiNet
* opcodes) is used instead of the NitrOS-9 tree's.

                    section   _constant

Level               equ       2
                    ifndef    H6309
H6309               equ       0
                    endc

                    use       os9.d
                    use       rbf.d
                    use       scf.d
                    use       pipe.d
                    use       rbsuper.d
                    use       drivewire.d

                    endsect
