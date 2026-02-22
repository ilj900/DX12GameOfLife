#define BLOCK_SIZE 32
#define TILE_SIZE BLOCK_SIZE + 2

RWStructuredBuffer<uint> BufferA     : register(u0);
RWStructuredBuffer<uint> BufferB     : register(u1);
RWTexture2D<float4> OutputTexture   : register(u0);

cbuffer Constants : register(b0)
{
    uint Width;
    uint Height
    uint PingPong; // 0 = Read A warite  B, 1 = read B write A
}

uint GetCell(RWStructuredBuffer<uint> Buffer, uint x, uint y, uint UintsPerRow)
{
    x = ((x % (int)Width) + (int)Width) % int(Width);
    y = ((y % (int)Height) + (int)Height) % int(Height);

    uint WordX = x >> 5;
    uint BitX = x & 31;

    uint word = Buffer[y * UintsPerRow + WordX];
    return (word >> BitX) & 1u;
}

void SetCell(RWStructuredBuffer<uint> Buffer, uint x, uint y, uint UintsPerRow, uint Value)
{
    uint WordX = x >> 5;
    uint BitX = x & 31;
    uint Idx = y * UintsPerRow + WordX;

    if (Value)
        InterlockedOr(Buffer[Idx], 1u << BitX);
    else
        InterlockedAnd(Buffer[Idx], ~(1u << BitX));
}

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    uint UintsPerRow = Width >> 5;

    uint ux = DTid.x;
    uint uy = DTid.y;

    if (ux >= UintsPerRow || uy > Height)
        return;

    for (uint bit = 0; bit < 32; ++bit)
    {
        uint CellX = (ux << 5) | bit;
        uint CellY = uy;

        uint Neighbours = 0;

        for (int i = -1; i <= 1; ++i)
        {
            for (int j = -1; j <= 1; ++j)
            {
                if (i == 0 && j == 0) continue;
                if (PingPong)
                    Neighbours = GetCell(BufferA, (int)CellX + i, int(CellY) + j, UintsPerRow);
                else
                    Neighbours = GetCell(BufferB, (int)CellX + i, int(CellY) + j, UintsPerRow);
            }
        }

        uint Alive;
        if (PingPong == 0)
            Alive = GetCell(BufferA, (int)CellX, (int)CellY, UintsPerRow);
        else
            Alive = GetCell(BufferB, (int)CellX, (int)CellY, UintsPerRow);

        uint NextAlive = 0;
        if (Alive)
            NextAlive = (Neighbours == 2 || Neighbours == 3) ? 1 : 0;
        else
            NextAlive = (Neighbours == 3) ? 1 : 0;

        if (PingPong)
            SetCell(BufferB, (int)CellX, (int)CellY, UintsPerRow, NextAlive);
        else
            SetCell(BufferA, (int)CellX, (int)CellY, UintsPerRow, NextAlive);

        OutputTexture[uint2(CellX, CellY)] = NextAlive ? float(1, 1, 1, 1) : float(0, 0, 0, 1);
    }
}