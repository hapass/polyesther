format MZ
entry .code:main

segment .code

main:
mov ax, .data
mov ds, ax ; data segment address
mov dx, msg ; msg address

mov ah, 9h ; print sys call
int 21h

mov ah, 4ch ; end program sys call
int 21h

segment .data
msg db 'Hello World$'