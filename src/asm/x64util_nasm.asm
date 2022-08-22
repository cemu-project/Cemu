DEFAULT REL

SECTION .text

global udiv128
global recompiler_fres
global recompiler_frsqrte

udiv128:
	mov rax, rcx
	div r8
	mov [r9], rdx
	ret

recompiler_fres:
 ; store all modified registers
push rdx
push rcx
push rax
push r8
lea         r8,[asmFresLookupTable]  
movq	    rdx, xmm15
mov         rcx,rdx  
shr         rcx,2Fh  
mov         rax,rdx  
and         ecx,1Fh  
shr         rax,25h  
and         eax,3FFh  
imul        eax,dword [r8+rcx*8+4]
mov         r8d,dword [r8+rcx*8]
mov         rcx,rdx  
shr         rcx,34h  
inc         eax  
shr         eax,1  
sub         r8d,eax  
and         ecx,7FFh  
jne         fres_espresso_label3
mov         rax,7FF0000000000000h  
or          rdx,rax  
movq		xmm15, rdx
pop r8
pop rax
pop rcx
pop rdx
ret  
fres_espresso_label3:
cmp         ecx,7FFh  
jne         fres_espresso_label4
mov         rax,0FFFFFFFFFFFFFh  
test        rax,rdx  
jne         fres_espresso_label1
test        rdx,rdx  
jns         fres_espresso_label2
mov         rax,8000000000000000h  
movq		xmm15, rax
pop r8
pop rax
pop rcx
pop rdx
ret  
fres_espresso_label2:
xorps       xmm15,xmm15  
pop r8
pop rax
pop rcx
pop rdx
ret  
fres_espresso_label4:
mov         eax,7FDh  
sub         eax,ecx  
mov         ecx,eax  
mov         rax,8000000000000000h  
and         rdx,rax  
shl         rcx,34h  
mov         eax,r8d  
or          rcx,rdx  
shl         rax,1Dh  
add         rcx,rax  
movq		xmm15, rcx
fres_espresso_label1:
pop r8
pop rax
pop rcx
pop rdx
ret  

asmFresLookupTable:
DD 07ff800h, 03e1h
DD 0783800h, 03a7h
DD 070ea00h, 0371h
DD 06a0800h, 0340h
DD 0638800h, 0313h
DD 05d6200h, 02eah
DD 0579000h, 02c4h
DD 0520800h, 02a0h
DD 04cc800h, 027fh
DD 047ca00h, 0261h
DD 0430800h, 0245h
DD 03e8000h, 022ah
DD 03a2c00h, 0212h
DD 0360800h, 01fbh
DD 0321400h, 01e5h
DD 02e4a00h, 01d1h
DD 02aa800h, 01beh
DD 0272c00h, 01ach
DD 023d600h, 019bh
DD 0209e00h, 018bh
DD 01d8800h, 017ch
DD 01a9000h, 016eh
DD 017ae00h, 015bh
DD 014f800h, 015bh
DD 0124400h, 0143h
DD 0fbe00h, 0143h
DD 0d3800h, 012dh
DD 0ade00h, 012dh
DD 088400h, 011ah
DD 065000h, 011ah
DD 041c00h, 0108h
DD 020c00h, 0106h

recompiler_frsqrte:
 ; store all modified registers
push rdx
push rcx
push rax
push r8
push r9
movq	    r8, xmm15
mov         rax,7FFFFFFFFFFFFFFFh  
test        rax,r8  
jne         frsqrte_espresso_label1
mov         rax,0FFF0000000000000h  
and         r8,rax  
mov         rax,7FF0000000000000h  
or          r8,rax  
movq		xmm15, r8
pop r9
pop r8
pop rax
pop rcx
pop rdx
ret  
frsqrte_espresso_label1:
mov         r9,r8  
shr         r9,34h  
and         r9d,7FFh  
cmp         r9d,7FFh  
jne         frsqrte_espresso_label2
mov         rax,0FFFFFFFFFFFFFh  
test        rax,r8  
jne         frsqrte_espresso_label3
test        r8,r8  
js          frsqrte_espresso_label4
xorps       xmm15,xmm15 
pop r9
pop r8
pop rax
pop rcx
pop rdx
ret  
frsqrte_espresso_label2:
test        r8,r8  
jns         frsqrte_espresso_label5
frsqrte_espresso_label4:
mov         rax,7FF8000000000000h  
movq		xmm15, rax
pop r9
pop r8
pop rax
pop rcx
pop rdx
ret  
frsqrte_espresso_label5:
lea         rdx,[asmFrsqrteLookupTable]  
mov         rax,r8  
shr         rax,30h  
mov         rcx,r8  
shr         rcx,25h  
and         eax,1Fh  
and         ecx,7FFh  
imul        ecx,dword [rdx+rax*8+4]
mov         eax,dword [rdx+rax*8]
sub         eax,ecx  
lea         ecx,[r9-3FDh]  
shr         ecx,1  
movsxd      rdx,eax  
mov         eax,3FFh  
sub         eax,ecx  
shl         rdx,1Ah  
mov         ecx,eax  
mov         rax,8000000000000000h  
and         r8,rax  
shl         rcx,34h  
or          rcx,r8  
add         rdx,rcx  
movq		xmm15, rdx
frsqrte_espresso_label3:
pop r9
pop r8
pop rax
pop rcx
pop rdx
ret  

asmFrsqrteLookupTable:
DD 01a7e800h, 0568h
DD 017cb800h, 04f3h
DD 01552800h, 048dh
DD 0130c000h, 0435h
DD 010f2000h, 03e7h
DD 0eff000h, 03a2h
DD 0d2e000h, 0365h
DD 0b7c000h, 032eh
DD 09e5000h, 02fch
DD 0867000h, 02d0h
DD 06ff000h, 02a8h
DD 05ab800h, 0283h
DD 046a000h, 0261h
DD 0339800h, 0243h
DD 0218800h, 0226h
DD 0105800h, 020bh
DD 03ffa000h, 07a4h
DD 03c29000h, 0700h
DD 038aa000h, 0670h
DD 03572000h, 05f2h
DD 03279000h, 0584h
DD 02fb7000h, 0524h
DD 02d26000h, 04cch
DD 02ac0000h, 047eh
DD 02881000h, 043ah
DD 02665000h, 03fah
DD 02468000h, 03c2h
DD 02287000h, 038eh
DD 020c1000h, 035eh
DD 01f12000h, 0332h
DD 01d79000h, 030ah
DD 01bf4000h, 02e6h
