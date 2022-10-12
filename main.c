#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>
#include <psxapi.h>
#include <psxetc.h>
#include <inline_c.h>

#define OT_LEN		4096
#define PACKET_LEN	161384

#define FLOOR_SIZE	40
#define ENEMY_COUNT	5

#define SCR_XRES	320
#define SCR_YRES	256

#define CENTERX		SCR_XRES >> 1
#define CENTERY		SCR_YRES >> 1

#define CUBE_FACES  6

#define CLIP_LEFT	1
#define CLIP_RIGHT	2
#define CLIP_TOP	4
#define CLIP_BOTTOM	8


typedef struct {
	DISPENV	disp;
	DRAWENV	draw;
	u_long 	orderTable[OT_LEN];
	char 	prim[PACKET_LEN];
} DB;

typedef struct {
	short v0, v1, v2, v3;
} INDEX;


DB	displayBuff[2];
int currentBuff = 0;

char padBuff[2][34];
char *nextPrim;

RECT screenClip;


SVECTOR cubeVerts[] = {
		{ -100, -100, -100, 0 },
		{  100, -100, -100, 0 },
		{ -100,  100, -100, 0 },
		{  100,  100, -100, 0 },
		{  100, -100,  100, 0 },
		{ -100, -100,  100, 0 },
		{  100,  100,  100, 0 },
		{ -100,  100,  100, 0 }
};

SVECTOR cubeNorms[] = {
		{ 0, 0, -ONE, 0 },
		{ 0, 0, ONE,  0 },
		{ 0, -ONE, 0, 0 },
		{ 0, ONE,  0, 0 },
		{ -ONE, 0, 0, 0 },
		{ ONE,  0, 0, 0 }
};

INDEX cubeIndices[] = {
		{ 0, 1, 2, 3 },
		{ 4, 5, 6, 7 },
		{ 5, 4, 0, 1 },
		{ 6, 7, 3, 2 },
		{ 0, 2, 5, 7 },
		{ 3, 1, 6, 4 }
};

MATRIX colorMtx = { ONE, 0, 0,   0, 0, 0,   ONE, 0, 0 };

MATRIX lightMtx = {
	/* X,  Y,  Z */
	-2048 , -2048 , -2048,
	 0	  ,  0	  ,  0,
	 0	  ,  0	  ,  0
};


void init();
void display();
void sortCube(MATRIX* mtx, VECTOR* pos, SVECTOR* rot, int r, int g, int b);
void crossProduct(SVECTOR *v0, SVECTOR *v1, VECTOR *out);
void lookAt(VECTOR *eye, VECTOR *at, SVECTOR *up, MATRIX *mtx);

int testClip(RECT *clip, short x, short y);
int quadClip(RECT *clip, DVECTOR *v0, DVECTOR *v1, DVECTOR *v2, DVECTOR *v3);


int main() {
	int i, p, xyTemp;
	int px, py;
	int lost = 0; int win = 0;

	SVECTOR	rot;			    VECTOR	pos;
	SVECTOR verts[FLOOR_SIZE + 1][FLOOR_SIZE + 1];
	VECTOR	camPos;		    	VECTOR	camRot;		    	int		camMode;
	VECTOR	tPos;			    SVECTOR	tRot;			    MATRIX	mtx, lmtx;
	PADTYPE* pad;
	POLY_F4* pol4;
	VECTOR victoryPos;
	SVECTOR victoryRot;


	victoryPos.vx = rand() % 2000+1000;
	victoryPos.vz = rand() % 2000+1000;
	victoryPos.vy = -125;

	victoryRot.vx = 0;
	victoryRot.vy = 0;

	init();

	pos.vx = 0;
	pos.vz = 0;
	pos.vz = -125;

	rot.vx = 0;
	rot.vz = 0;
	rot.vy = 0;

	for(py = 0; py < FLOOR_SIZE+1; py++) {
		for(px = 0; px < FLOOR_SIZE+1; px++) {
			setVector(&verts[py][px],
				(100 * (px-8)) - 50,
				 0,
				(100 * (py-8)) - 50);
		}
	}

	setVector(&camPos, 0, ONE * -250, 0);
	setVector(&camRot, 0, 0, 0);

	while(1) {
		pad = (PADTYPE*)&padBuff[0][0];
		camMode = 0;

		tRot.vx = camRot.vx >> 12;
		tRot.vy = camRot.vy >> 12;
		tRot.vz = camRot.vz >> 12;

		if (pad->stat == 0 && !lost && !win) {
			if((pad->type == 0x5) || (pad->type == 0x7)) {
				if(((pad->ls_y - 128) < -16) || ((pad->ls_y - 128) > 16)) {
					pos.vz -= (((pad->ls_y - 128) / 64) * (isin(-(rot.vy + 1024)) / 1000));
					pos.vx -= (((pad->ls_y - 128) / 64) * (icos(-(rot.vy + 1024)) / 1000));
				}

				if(((pad->ls_x - 128) < -16) || ((pad->ls_x - 128) > 16)) {
					pos.vz -= (((pad->ls_x - 128) / 64) * (isin(-(rot.vy)) / 1000));
					pos.vx -= (((pad->ls_x - 128) / 64) * (icos(-(rot.vy)) / 1000));
				}

				if(((pad->rs_x - 128) < -16) || ((pad->rs_x - 128) > 16)) {
					rot.vy += (pad->rs_x - 128) / 10;
				}
			}
		}


		SVECTOR up = { 0, -ONE, 0 };

		tPos.vx = camPos.vx >> 12;
		tPos.vz = camPos.vz >> 12;
		tPos.vy = camPos.vy >> 12;

		lookAt(&tPos, &pos, &up, &mtx);

		gte_SetRotMatrix(&mtx);
		gte_SetTransMatrix(&mtx);

		pol4 = (POLY_F4 *)nextPrim;

		for(py = 0; py < FLOOR_SIZE; py++) {
			for(px = 0; px < FLOOR_SIZE; px++) {
				gte_ldv3(
					&verts[py][px],
					&verts[py][px+1],
					&verts[py+1][px]
				);

				gte_rtpt();
				gte_avsz3();
				gte_stotz(&p);

				if(((p >> 2) >= OT_LEN) || ((p >> 2) <= 0)) continue;

				setPolyF4(pol4);

				gte_stsxy0(&pol4->x0);
				gte_stsxy1(&pol4->x1);
				gte_stsxy2(&pol4->x2);

				gte_ldv0(&verts[py+1][px+1]);
				gte_rtps();
				gte_stsxy(&pol4->x3);

				if(quadClip(&screenClip,
					(DVECTOR*)&pol4->x0, (DVECTOR*)&pol4->x1,
					(DVECTOR*)&pol4->x2, (DVECTOR*)&pol4->x3
				)) continue;

				gte_avsz4();
				gte_stotz(&p);

				if((px+py) & 0x1) setRGB0(pol4, 128, 128, 128);
				else 			  setRGB0(pol4, 255, 255, 255);
			
				addPrim(displayBuff[currentBuff].orderTable + (p >> 2), pol4);
				pol4++;
			}
		}

		nextPrim = (char*)pol4;
		sortCube(&mtx, &pos, 		&rot, 		 0  , 200, 0);
		sortCube(&mtx, &victoryPos, &victoryRot, 200, 200, 0);

		camPos.vx = (pos.vx + (isin(rot.vy) - 1) / 10) << 12;
		camPos.vz = (pos.vz + (isin(rot.vy) - 1) / 10) << 12;

		victoryRot.vx = victoryRot.vx + 10;
		victoryRot.vy = victoryRot.vy + 10;

		FntFlush(-1);
		display();
	} return 0;
}


void init() {
	ResetGraph(0);
	
	SetDefDispEnv(&displayBuff[0].disp, 0, 		   0,  SCR_XRES, SCR_YRES);
	SetDefDrawEnv(&displayBuff[0].draw, SCR_XRES,  0,  SCR_XRES, SCR_YRES);

	setRGB0(&displayBuff[0].draw, 0, 0, 255);
	displayBuff[0].draw.isbg = 1;
	displayBuff[0].draw.dtd  = 1;


	SetDefDispEnv(&displayBuff[1].disp, SCR_XRES,  0,  SCR_XRES, SCR_YRES);
	SetDefDrawEnv(&displayBuff[1].draw, 0, 		   0,  SCR_XRES, SCR_YRES);

	setRGB0(&displayBuff[1].draw, 0, 0, 255);
	displayBuff[1].draw.isbg = 1;
	displayBuff[1].draw.dtd  = 1;


	SetVideoMode(MODE_PAL);
	PutDrawEnv(&displayBuff[0].draw);
	
	ClearOTagR(displayBuff[0].orderTable, OT_LEN);
	ClearOTagR(displayBuff[1].orderTable, OT_LEN);

	nextPrim = displayBuff[0].prim;

	setRECT(&screenClip, 0, 0, SCR_XRES, SCR_YRES);


	InitGeom();
		gte_SetGeomOffset(CENTERX, CENTERY);
		gte_SetGeomScreen(CENTERX);
		gte_SetBackColor(0, 255, 0);
		gte_SetColorMatrix(&colorMtx);


	InitPAD(&padBuff[0][0], 34, &padBuff[1][0], 34);
	StartPAD();
	ChangeClearPAD(0);

	FntLoad(960, 0);
	FntOpen(0, 8, 320, 216, 0, 100);

	return;
}

void display() {
	DrawSync(0);
	VSync(0);

	currentBuff = !currentBuff;
	nextPrim = displayBuff[currentBuff].prim;

	ClearOTagR(displayBuff[currentBuff].orderTable, OT_LEN);

	PutDrawEnv(&displayBuff[currentBuff].draw);
	PutDispEnv(&displayBuff[currentBuff].disp);

	SetDispMask(1);
	DrawOTag(displayBuff[1 - currentBuff].orderTable + (OT_LEN-1));

	return;
}

void sortCube(MATRIX* mtx, VECTOR* pos, SVECTOR* rot, int r, int g, int b) {

	int i, p;
	POLY_F4* pol4;

	MATRIX omtx, lmtx;

	RotMatrix(rot, &omtx);
	TransMatrix(&omtx, pos);
	MulMatrix0(&lightMtx, &omtx, &lmtx);

	gte_SetLightMatrix(&lmtx);

	CompMatrixLV(mtx, &omtx, &omtx);
	PushMatrix();

	gte_SetRotMatrix(&omtx);
	gte_SetTransMatrix(&omtx);

	pol4 = (POLY_F4*)nextPrim;

	for(i = 0; i < CUBE_FACES; i++) {
		gte_ldv3(
			&cubeVerts[cubeIndices[i].v0],
			&cubeVerts[cubeIndices[i].v1],
			&cubeVerts[cubeIndices[i].v2]);

		gte_rtpt();
		gte_nclip();
		gte_stopz(&p);

		if(p < 0) continue;

		gte_avsz3();
		gte_stotz(&p);

		if(((p >> 2) <= 0) || ((p >> 2) >= OT_LEN)) continue;

		setPolyF4(pol4);

		gte_stsxy0(&pol4->x0);
		gte_stsxy1(&pol4->x1);
		gte_stsxy2(&pol4->x2);

		gte_ldv0(&cubeVerts[cubeIndices[i].v3]);
		gte_rtps();
		gte_stsxy(&pol4->x3);

		gte_ldrgb(&pol4->r0);
		gte_ldv0(&cubeNorms[i]);

		gte_ncs();
		gte_strgb(&pol4->r0);

		gte_avsz4();
		gte_stotz(&p);

		setRGB0(pol4, r, g, b);
		addPrim(displayBuff[currentBuff].orderTable + (p >> 2), pol4);

		pol4++;
	}

	nextPrim = (char*)pol4;
	PopMatrix();

	return;
}

void crossProduct(SVECTOR *v0, SVECTOR *v1, VECTOR *out) {
	out->vx = ((v0->vy*v1->vz) - (v0->vz*v1->vy)) >> 12;
	out->vy = ((v0->vz*v1->vx) - (v0->vx*v1->vz)) >> 12;
	out->vz = ((v0->vx*v1->vy) - (v0->vy*v1->vx)) >> 12;

	return;
}

void lookAt(VECTOR *eye, VECTOR *at, SVECTOR *up, MATRIX *mtx) {

	VECTOR tAxis;
	SVECTOR zAxis;
	SVECTOR xAxis;
	SVECTOR yAxis;
	VECTOR pos;
	VECTOR vec;

	setVector(&tAxis, at->vx - eye->vx,  at->vy - eye->vy,  at->vz - eye->vz);
		VectorNormalS(&tAxis, &zAxis);
    	crossProduct(&zAxis, up, &tAxis);
	
		VectorNormalS(&tAxis, &xAxis);
		crossProduct(&zAxis, &xAxis, &tAxis);
	
		VectorNormalS(&tAxis, &yAxis);

	mtx->m[0][0] = xAxis.vx;	mtx->m[1][0] = yAxis.vx;	mtx->m[2][0] = zAxis.vx;
	mtx->m[0][1] = xAxis.vy;	mtx->m[1][1] = yAxis.vy;	mtx->m[2][1] = zAxis.vy;
	mtx->m[0][2] = xAxis.vz;	mtx->m[1][2] = yAxis.vz;	mtx->m[2][2] = zAxis.vz;

	pos.vx = -eye->vx;
	pos.vy = -eye->vy;
	pos.vz = -eye->vz;

	ApplyMatrixLV(mtx, &pos, &vec);
	TransMatrix(mtx, &vec);

	return;
}


int testClip(RECT *clip, short x, short y) {
	int result = 0;

	if(x <   clip->x)  			      result |= CLIP_LEFT;
	if(x >= (clip->x + (clip->w-1)))  result |= CLIP_RIGHT;
	
	if(y <   clip->y) 			      result |= CLIP_TOP;
	if(y >= (clip->y + (clip->h-1)))  result |= CLIP_BOTTOM;

	return result;
}

int quadClip(RECT *clip, DVECTOR *v0, DVECTOR *v1, DVECTOR *v2, DVECTOR *v3) {
	short c[4];

	c[0] = testClip(clip, v0->vx, v0->vy);
	c[1] = testClip(clip, v1->vx, v1->vy);
	c[2] = testClip(clip, v2->vx, v2->vy);
	c[3] = testClip(clip, v3->vx, v3->vy);

	if((c[0] & c[1]) == 0) return 0;
	if((c[1] & c[2]) == 0) return 0;
	if((c[2] & c[3]) == 0) return 0;
	if((c[3] & c[0]) == 0) return 0;
	if((c[0] & c[2]) == 0) return 0;
	if((c[1] & c[3]) == 0) return 0;

	return 1;
}