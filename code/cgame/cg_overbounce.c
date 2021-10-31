#include "cg_local.h"


#define OB_GRID_SIZE    20
#define OB_BOX_SIZE     15
#define OB_MAX_LEVELS   200

#define OB_MARK_GRID_SIZE       2
#define OB_MARK_QUAD_SIZE       200
#define	OB_MAX_MARK_FRAGMENTS	128
#define	OB_MAX_MARK_POINTS		384
#define OB_PROJECTION_DEPTH     0.01


typedef enum {
    OBTYPE_NONE,
    OBTYPE_DROP,
    OBTYPE_JUMP,
    OBTYPE_DROP_STICKY,
    OBTYPE_JUMP_STICKY,
    OBTYPE_TRAJECTORY,
} obType_t;


typedef struct {
    float height;
    obType_t obType;
} obLevel_t;


static obLevel_t obLevels[OB_MAX_LEVELS];
static int numObLevels = 0;


static qboolean CG_IsOverbounceHeight(float height, float vel) {
    float step_num;
    float a, b, c;
    float pos;

    a = -6 * 0.008 / 2;
    b = vel * 0.008 - 800 * 0.008 * 0.008 / 2 - a;
    c = height;

    step_num = ceil((-b - sqrt(b * b  - 4 * a * (c - 0.25))) / (2 * a));
    pos = a * step_num * step_num + b * step_num + c;

    return pos > 0;
}


static void CG_OverbounceInsertMark (float height, obType_t obType) {
    int levelNum;
    obLevel_t *level;

    for (levelNum = 0, level=obLevels; levelNum < numObLevels; levelNum++, level++) {
        if (abs(height - level->height) < 0.01f) {
            break;
        }
    }

    if (levelNum < OB_MAX_LEVELS) {
        if (levelNum == numObLevels) {
            level->height = height;
            level->obType = obType;
            numObLevels++;
        }
    }
}


static void CG_OverbounceInsertMarks (vec3_t origin, vec3_t vel) {
	vec3_t start, end;
    vec3_t mins = {-15, -15, -24};
    vec3_t maxs = {15, 15, 32};
    int grid_x, grid_y;
	trace_t	trace;
    obType_t obType;

    numObLevels = 0;
    for (grid_x = -OB_GRID_SIZE; grid_x <= OB_GRID_SIZE; grid_x++) {
        for (grid_y = -OB_GRID_SIZE; grid_y <= OB_GRID_SIZE; grid_y++) {
            VectorCopy ( origin, start );
            start[0] += grid_x * OB_BOX_SIZE * 2;
            start[1] += grid_y * OB_BOX_SIZE * 2;
            start[2] -= 16;

            VectorCopy( start, end );
            end[2] -= 8192;

            trap_CM_BoxTrace( &trace, start, end, mins, maxs, 0, MASK_PLAYERSOLID );
            if ( trace.fraction == 1.0 || trace.startsolid || trace.allsolid ||
                 trace.plane.normal[2] < 0.999f) {
                continue;
            }

            obType = OBTYPE_NONE;
            if (cg.snap->ps.groundEntityNum == ENTITYNUM_NONE) {
                if (CG_IsOverbounceHeight(origin[2] - trace.endpos[2], vel[2])) {
                    obType = OBTYPE_TRAJECTORY;
                }
            } else if (CG_IsOverbounceHeight(origin[2] - trace.endpos[2], 0)) {
                obType = OBTYPE_DROP;
            } else if(CG_IsOverbounceHeight(origin[2] - trace.endpos[2], 270)) {
                obType = OBTYPE_JUMP;
            } else if(CG_IsOverbounceHeight(0.25 + origin[2] - trace.endpos[2], 0)) {
                obType = OBTYPE_DROP_STICKY;
            } else if (CG_IsOverbounceHeight(0.25 + origin[2] - trace.endpos[2], 270)) {
                obType = OBTYPE_JUMP_STICKY;
            }

            if (obType != OBTYPE_NONE) {
                CG_OverbounceInsertMark(trace.endpos[2] - 24 - 0.125, obType);
            }
        }
    }
}


static void CG_OverbounceDrawMark ( qhandle_t shader, byte color[4], vec3_t pos ) {
    int i, j;
	vec3_t originalPoints[4];
	int				numFragments;
	markFragment_t	markFragments[OB_MAX_MARK_FRAGMENTS], *mf;
	vec3_t			markPoints[OB_MAX_MARK_POINTS];
	vec3_t			projection = {0, 0, -OB_PROJECTION_DEPTH};

    for (i = 0; i < 4; i++) {
        VectorCopy(pos, originalPoints[i]);
    }
    originalPoints[0][0] -= OB_MARK_QUAD_SIZE; originalPoints[0][1] += OB_MARK_QUAD_SIZE;
    originalPoints[1][0] += OB_MARK_QUAD_SIZE; originalPoints[1][1] += OB_MARK_QUAD_SIZE;
    originalPoints[2][0] += OB_MARK_QUAD_SIZE; originalPoints[2][1] -= OB_MARK_QUAD_SIZE;
    originalPoints[3][0] -= OB_MARK_QUAD_SIZE; originalPoints[3][1] -= OB_MARK_QUAD_SIZE;

	numFragments = trap_CM_MarkFragments( 4, (void *)originalPoints,
					projection, OB_MAX_MARK_POINTS, markPoints[0],
					OB_MAX_MARK_FRAGMENTS, markFragments );

	for ( i = 0, mf = markFragments ; i < numFragments ; i++, mf++ ) {
		polyVert_t	*v;
		polyVert_t	verts[MAX_VERTS_ON_POLY];

		if ( mf->numPoints > MAX_VERTS_ON_POLY ) {
			mf->numPoints = MAX_VERTS_ON_POLY;
		}

		for ( j = 0, v = verts ; j < mf->numPoints ; j++, v++ ) {
			VectorCopy( markPoints[mf->firstPoint + j], v->xyz );
            if (abs(v->xyz[2] - pos[2]) > 0.01) {
                break;
            }

			v->st[0] = v->xyz[0] / 16;
			v->st[1] = v->xyz[1] / 16;
			*(int *)v->modulate = *(int *)color;
		}

        if (j == mf->numPoints) {
			trap_R_AddPolyToScene( shader, mf->numPoints, verts );
        }
    }
}


static void CG_OverbounceDrawMarks (vec3_t origin) {
    obLevel_t *level;
    int levelNum, grid_x, grid_y;
    byte color[4];
    obType_t levelObType, obType;
    vec3_t markPos;
    vec3_t normal = {0, 0, 1};
    qhandle_t shader;

    for (level = obLevels, levelNum = 0; levelNum < numObLevels; level++, levelNum++) {
        switch (level->obType) {
            case OBTYPE_DROP:
                color[0] = 0; color[1] = 0; color[2] = 255; color[3] = 255;
                shader = cgs.media.obMarkerShader;
                break;
            case OBTYPE_JUMP:
                color[0] = 255; color[1] = 255; color[2] = 0; color[3] = 255;
                shader = cgs.media.obMarkerShader;
                break;
            case OBTYPE_DROP_STICKY:
                color[0] = 0; color[1] = 0; color[2] = 255; color[3] = 255;
                shader = cgs.media.obMarkerStickyShader;
                break;
            case OBTYPE_JUMP_STICKY:
                color[0] = 255; color[1] = 255; color[2] = 0; color[3] = 255;
                shader = cgs.media.obMarkerStickyShader;
                break;
            case OBTYPE_TRAJECTORY:
                color[0] = 255; color[1] = 255; color[2] = 255; color[3] = 255;
                shader = cgs.media.obMarkerShader;
                break;
        }

        for (grid_x = -OB_MARK_GRID_SIZE; grid_x <= OB_MARK_GRID_SIZE; grid_x++) {
            for (grid_y = -OB_MARK_GRID_SIZE; grid_y <= OB_MARK_GRID_SIZE; grid_y++) {
                VectorCopy ( origin, markPos );
                markPos[0] += OB_MARK_QUAD_SIZE * 2 * grid_x;
                markPos[1] += OB_MARK_QUAD_SIZE * 2 * grid_y;
                markPos[2] = level->height;

                CG_OverbounceDrawMark(shader, color, markPos);
            }
        }
    }
}


void CG_OverbounceCheck(void) {
    vec3_t origin, vel;

    if (cg_overbounce_hud.value) {
        VectorCopy( cg.snap->ps.origin, origin );
        VectorCopy( cg.snap->ps.velocity, vel );

        CG_OverbounceInsertMarks(origin, vel);
        CG_OverbounceDrawMarks(origin);
    }
}


