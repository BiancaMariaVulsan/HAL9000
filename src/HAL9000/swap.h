#pragma once

typedef QWORD SWAP_ADDRESS, *PSWAP_ADDRESS;

void
SwapSystemPreinit(
    void
);

void
SwapSystemInit(
    void
);

STATUS
SwapOut(
    IN          PVOID               VirtualAddress,
    OUT         PSWAP_ADDRESS       SwapAddress
);