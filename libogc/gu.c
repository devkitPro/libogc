#include <gu.h>
#include <math.h>

extern void __ps_guMtxRotAxisRadInternal(register Mtx mt,const register Vector *axis,register f32 sT,register f32 cT);

void guFrustum(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f)
{
	f32 tmp;

	tmp = 1.0f/(r-l);
	mt[0][0] = (2*n)*tmp;
	mt[0][1] = 0.0f;
	mt[0][2] = (r+l)*tmp;
	mt[0][3] = 0.0f;

	tmp = 1.0f/(t-b);
	mt[1][0] = 0.0f;
	mt[1][1] = (2*n)*tmp;
	mt[1][2] = (t+b)*tmp;
	mt[1][3] = 0.0f;

	tmp = 1.0f/(f-n);
	mt[2][0] = 0.0f;
	mt[2][1] = 0.0f;
	mt[2][2] = -(n)*tmp;
	mt[2][3] = -(f*n)*tmp;

	mt[3][0] = 0.0f;
	mt[3][1] = 0.0f;
	mt[3][2] = -1.0f;
	mt[3][3] = 0.0f;
}

void guPerspective(Mtx44 mt,f32 fovy,f32 aspect,f32 n,f32 f)
{
	f32 cot,angle,tmp;

	angle = fovy*0.5f;
	angle = DegToRad(angle);
	
	cot = 1.0f/tanf(angle);

	mt[0][0] = cot/aspect;
	mt[0][1] = 0.0f;
	mt[0][2] = 0.0f;
	mt[0][3] = 0.0f;

	mt[1][0] = 0.0f;
	mt[1][1] = cot;
	mt[1][2] = 0.0f;
	mt[1][3] = 0.0f;
	
	tmp = 1.0f/(f-n);
	mt[2][0] = 0.0f;
	mt[2][1] = 0.0f;
	mt[2][2] = -(n)*tmp;
	mt[2][3] = -(f*n)*tmp;
	
	mt[3][0] = 0.0f;
	mt[3][1] = 0.0f;
	mt[3][2] = -1.0f;
	mt[3][3] = 0.0f;
}

void guOrtho(Mtx44 mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 f)
{
	f32 tmp;

	tmp = 1.0f/(r-l);
	mt[0][0] = 2.0f*tmp;
	mt[0][1] = 0.0f;
	mt[0][2] = 0.0f;
	mt[0][3] = -(r+l)*tmp;

	tmp = 1.0f/(t-b);
	mt[1][0] = 0.0f;
	mt[1][1] = 2.0f*tmp;
	mt[1][2] = 0.0f;
	mt[1][3] = -(t+b)*tmp;

	tmp = 1.0f/(f-n);
	mt[2][0] = 0.0f;
	mt[2][1] = 0.0f;
	mt[2][2] = -(1.0f)*tmp;
	mt[2][3] = -(f)*tmp;

	mt[3][0] = 0.0f;
	mt[3][1] = 0.0f;
	mt[3][2] = 0.0f;
	mt[3][3] = 1.0f;
}

void guLightPerspective(Mtx mt,f32 fovY,f32 aspect,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
{
	f32 angle;
	f32 cot;

	angle = fovY*0.5f;
	angle = DegToRad(angle);
	
	cot = 1.0f/tanf(angle);

    mt[0][0] =    (cot / aspect) * scaleS;
    mt[0][1] =    0.0f;
    mt[0][2] =    -transS;
    mt[0][3] =    0.0f;

    mt[1][0] =    0.0f;
    mt[1][1] =    cot * scaleT;
    mt[1][2] =    -transT;
    mt[1][3] =    0.0f;

    mt[2][0] =    0.0f;
    mt[2][1] =    0.0f;
    mt[2][2] =   -1.0f;
    mt[2][3] =    0.0f;
}

void guLightOrtho(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
{
	f32 tmp;

    tmp     =  1.0f / (r - l);
    mt[0][0] =  (2.0f * tmp * scaleS);
    mt[0][1] =  0.0f;
    mt[0][2] =  0.0f;
    mt[0][3] =  ((-(r + l) * tmp) * scaleS) + transS;

    tmp     =  1.0f / (t - b);
    mt[1][0] =  0.0f;
    mt[1][1] =  (2.0f * tmp) * scaleT;
    mt[1][2] =  0.0f;
    mt[1][3] =  ((-(t + b) * tmp)* scaleT) + transT;

    mt[2][0] =  0.0f;
    mt[2][1] =  0.0f;
    mt[2][2] =  0.0f;
    mt[2][3] =  1.0f;
}

void guLightFrustum(Mtx mt,f32 t,f32 b,f32 l,f32 r,f32 n,f32 scaleS,f32 scaleT,f32 transS,f32 transT)
{
    f32 tmp;
    
    tmp     =  1.0f / (r - l);
    mt[0][0] =  ((2*n) * tmp) * scaleS;
    mt[0][1] =  0.0f;
    mt[0][2] =  (((r + l) * tmp) * scaleS) - transS;
    mt[0][3] =  0.0f;

    tmp     =  1.0f / (t - b);
    mt[1][0] =  0.0f;
    mt[1][1] =  ((2*n) * tmp) * scaleT;
    mt[1][2] =  (((t + b) * tmp) * scaleT) - transT;
    mt[1][3] =  0.0f;

    mt[2][0] =  0.0f;
    mt[2][1] =  0.0f;
    mt[2][2] = -1.0f;
    mt[2][3] =  0.0f;
}

void guLookAt(Mtx mt,Vector *camPos,Vector *camUp,Vector *target)
{
	Vector vLook,vRight,vUp;

	vLook.x = camPos->x - target->x;
	vLook.y = camPos->y - target->y;
	vLook.z = camPos->z - target->z;
	guVecNormalize(&vLook);

	guVecCross(camUp,&vLook,&vRight);
	guVecNormalize(&vRight);
	
	guVecCross(&vLook,&vRight,&vUp);

    mt[0][0] = vRight.x;
    mt[0][1] = vRight.y;
    mt[0][2] = vRight.z;
    mt[0][3] = -( camPos->x * vRight.x + camPos->y * vRight.y + camPos->z * vRight.z );

    mt[1][0] = vUp.x;
    mt[1][1] = vUp.y;
    mt[1][2] = vUp.z;
    mt[1][3] = -( camPos->x * vUp.x + camPos->y * vUp.y + camPos->z * vUp.z );

    mt[2][0] = vLook.x;
    mt[2][1] = vLook.y;
    mt[2][2] = vLook.z;
    mt[2][3] = -( camPos->x * vLook.x + camPos->y * vLook.y + camPos->z * vLook.z );
}

void c_guMtxIdentity(Mtx mt)
{
	s32 i,j;

	for(i=0;i<3;i++) {
		for(j=0;j<4;j++) {
			if(i==j) mt[i][j] = 1.0;
			else mt[i][j] = 0.0;
		}
	}
}

void c_guMtxRotRad(Mtx mt,const char axis,f32 rad)
{
	f32 sinA,cosA;

	sinA = sinf(rad);
	cosA = cosf(rad);

	c_guMtxRotTrig(mt,axis,sinA,cosA);
}

#ifdef GEKKO
void ps_guMtxRotRad(register Mtx mt,const register char axis,register f32 rad)
{
	register f32 sinA,cosA;

	sinA = sinf(rad);
	cosA = cosf(rad);

	ps_guMtxRotTrig(mt,axis,sinA,cosA);
}

void ps_guMtxRotAxisRad(Mtx mt,Vector *axis,f32 rad)
{
	f32 sinT,cosT;
 
	sinT = sinf(rad);
	cosT = cosf(rad);
 
	__ps_guMtxRotAxisRadInternal(mt,axis,sinT,cosT);
}

#endif

void c_guMtxRotTrig(Mtx mt,const char axis,f32 sinA,f32 cosA)
{
	switch(axis) {
		case 'x':
		case 'X':
			mt[0][0] =  1.0f;  mt[0][1] =  0.0f;    mt[0][2] =  0.0f;  mt[0][3] = 0.0f;
			mt[1][0] =  0.0f;  mt[1][1] =  cosA;    mt[1][2] = -sinA;  mt[1][3] = 0.0f;
			mt[2][0] =  0.0f;  mt[2][1] =  sinA;    mt[2][2] =  cosA;  mt[2][3] = 0.0f;
			break;
		case 'y':
		case 'Y':
			mt[0][0] =  cosA;  mt[0][1] =  0.0f;    mt[0][2] =  sinA;  mt[0][3] = 0.0f;
			mt[1][0] =  0.0f;  mt[1][1] =  1.0f;    mt[1][2] =  0.0f;  mt[1][3] = 0.0f;
			mt[2][0] = -sinA;  mt[2][1] =  0.0f;    mt[2][2] =  cosA;  mt[2][3] = 0.0f;
			break;
		case 'z':
		case 'Z':
			mt[0][0] =  cosA;  mt[0][1] = -sinA;    mt[0][2] =  0.0f;  mt[0][3] = 0.0f;
			mt[1][0] =  sinA;  mt[1][1] =  cosA;    mt[1][2] =  0.0f;  mt[1][3] = 0.0f;
			mt[2][0] =  0.0f;  mt[2][1] =  0.0f;    mt[2][2] =  1.0f;  mt[2][3] = 0.0f;
			break;
		default:
			break;
	}
}

void c_guMtxRotAxisRad(Mtx mt,Vector *axis,f32 rad)
{
	f32 s,c;
	f32 t;
	f32 x,y,z;
	f32 xSq,ySq,zSq;
	
	s = sinf(rad);
	c = cosf(rad);
	t = 1.0f-c;
	
	c_guVecNormalize(axis);
	
	x = axis->x;
	y = axis->y;
	z = axis->z;

	xSq = x*x;
	ySq = y*y;
	zSq = z*z;

    mt[0][0] = ( t * xSq )   + ( c );
    mt[0][1] = ( t * x * y ) - ( s * z );
    mt[0][2] = ( t * x * z ) + ( s * y );
    mt[0][3] =    0.0f;

    mt[1][0] = ( t * x * y ) + ( s * z );
    mt[1][1] = ( t * ySq )   + ( c );
    mt[1][2] = ( t * y * z ) - ( s * x );
    mt[1][3] =    0.0f;

    mt[2][0] = ( t * x * z ) - ( s * y );
    mt[2][1] = ( t * y * z ) + ( s * x );
    mt[2][2] = ( t * zSq )   + ( c );
    mt[2][3] =    0.0f;

}

void c_guMtxCopy(Mtx src,Mtx dst)
{
	if(src==dst) return;

    dst[0][0] = src[0][0];    dst[0][1] = src[0][1];    dst[0][2] = src[0][2];    dst[0][3] = src[0][3];
    dst[1][0] = src[1][0];    dst[1][1] = src[1][1];    dst[1][2] = src[1][2];    dst[1][3] = src[1][3];
    dst[2][0] = src[2][0];    dst[2][1] = src[2][1];    dst[2][2] = src[2][2];    dst[2][3] = src[2][3];
}

void c_guMtxConcat(Mtx a,Mtx b,Mtx ab)
{
	Mtx tmp;
	MtxP m;

	if(ab==b || ab==a)
		m = tmp;
	else
		m = ab;

    m[0][0] = a[0][0]*b[0][0] + a[0][1]*b[1][0] + a[0][2]*b[2][0];
    m[0][1] = a[0][0]*b[0][1] + a[0][1]*b[1][1] + a[0][2]*b[2][1];
    m[0][2] = a[0][0]*b[0][2] + a[0][1]*b[1][2] + a[0][2]*b[2][2];
    m[0][3] = a[0][0]*b[0][3] + a[0][1]*b[1][3] + a[0][2]*b[2][3] + a[0][3];

    m[1][0] = a[1][0]*b[0][0] + a[1][1]*b[1][0] + a[1][2]*b[2][0];
    m[1][1] = a[1][0]*b[0][1] + a[1][1]*b[1][1] + a[1][2]*b[2][1];
    m[1][2] = a[1][0]*b[0][2] + a[1][1]*b[1][2] + a[1][2]*b[2][2];
    m[1][3] = a[1][0]*b[0][3] + a[1][1]*b[1][3] + a[1][2]*b[2][3] + a[1][3];

    m[2][0] = a[2][0]*b[0][0] + a[2][1]*b[1][0] + a[2][2]*b[2][0];
    m[2][1] = a[2][0]*b[0][1] + a[2][1]*b[1][1] + a[2][2]*b[2][1];
    m[2][2] = a[2][0]*b[0][2] + a[2][1]*b[1][2] + a[2][2]*b[2][2];
    m[2][3] = a[2][0]*b[0][3] + a[2][1]*b[1][3] + a[2][2]*b[2][3] + a[2][3];

	if(m==tmp)
		c_guMtxCopy(tmp,ab);
}

void c_guMtxScale(Mtx mt,f32 xS,f32 yS,f32 zS)
{
    mt[0][0] = xS;    mt[0][1] = 0.0f;  mt[0][2] = 0.0f;  mt[0][3] = 0.0f;
    mt[1][0] = 0.0f;  mt[1][1] = yS;    mt[1][2] = 0.0f;  mt[1][3] = 0.0f;
    mt[2][0] = 0.0f;  mt[2][1] = 0.0f;  mt[2][2] = zS;    mt[2][3] = 0.0f;
}

void c_guMtxScaleApply(Mtx src,Mtx dst,f32 xS,f32 yS,f32 zS)
{
	dst[0][0] = src[0][0] * xS;     dst[0][1] = src[0][1] * xS;
	dst[0][2] = src[0][2] * xS;     dst[0][3] = src[0][3] * xS;

	dst[1][0] = src[1][0] * yS;     dst[1][1] = src[1][1] * yS;
	dst[1][2] = src[1][2] * yS;     dst[1][3] = src[1][3] * yS;

	dst[2][0] = src[2][0] * zS;     dst[2][1] = src[2][1] * zS;
	dst[2][2] = src[2][2] * zS;     dst[2][3] = src[2][3] * zS;
}

void c_guMtxApplyScale(Mtx src,Mtx dst,f32 xS,f32 yS,f32 zS)
{
	dst[0][0] = src[0][0] * xS;     dst[0][1] = src[0][1] * yS;
	dst[0][2] = src[0][2] * zS;     dst[0][3] = src[0][3];

	dst[1][0] = src[1][0] * xS;     dst[1][1] = src[1][1] * yS;
	dst[1][2] = src[1][2] * zS;     dst[1][3] = src[1][3];

	dst[2][0] = src[2][0] * xS;     dst[2][1] = src[2][1] * yS;
	dst[2][2] = src[2][2] * zS;     dst[2][3] = src[2][3];
}

void c_guMtxTrans(Mtx mt,f32 xT,f32 yT,f32 zT)
{
    mt[0][0] = 1.0f;  mt[0][1] = 0.0f;  mt[0][2] = 0.0f;  mt[0][3] =  xT;
    mt[1][0] = 0.0f;  mt[1][1] = 1.0f;  mt[1][2] = 0.0f;  mt[1][3] =  yT;
    mt[2][0] = 0.0f;  mt[2][1] = 0.0f;  mt[2][2] = 1.0f;  mt[2][3] =  zT;
}

void c_guMtxTransApply(Mtx src,Mtx dst,f32 xT,f32 yT,f32 zT)
{
	if ( src != dst )
	{
		dst[0][0] = src[0][0];    dst[0][1] = src[0][1];    dst[0][2] = src[0][2];
		dst[1][0] = src[1][0];    dst[1][1] = src[1][1];    dst[1][2] = src[1][2];
		dst[2][0] = src[2][0];    dst[2][1] = src[2][1];    dst[2][2] = src[2][2];
	}

	dst[0][3] = src[0][3] + xT;
	dst[1][3] = src[1][3] + yT;
	dst[2][3] = src[2][3] + zT;
}

void c_guMtxApplyTrans(Mtx src,Mtx dst,f32 xT,f32 yT,f32 zT)
{
	if ( src != dst )
	{
		dst[0][0] = src[0][0];    dst[0][1] = src[0][1];    dst[0][2] = src[0][2];
		dst[1][0] = src[1][0];    dst[1][1] = src[1][1];    dst[1][2] = src[1][2];
		dst[2][0] = src[2][0];    dst[2][1] = src[2][1];    dst[2][2] = src[2][2];
	}

	dst[0][3] = src[0][0]*xT + src[0][1]*yT + src[0][2]*zT + src[0][3];
	dst[1][3] = src[1][0]*xT + src[1][1]*yT + src[1][2]*zT + src[1][3];
	dst[2][3] = src[2][0]*xT + src[2][1]*yT + src[2][2]*zT + src[2][3];
}

u32 c_guMtxInverse(Mtx src,Mtx inv)
{
    Mtx mTmp;
    MtxP m;
    f32 det;

    if(src==inv)
        m = mTmp;
	else 
        m = inv;


    // compute the determinant of the upper 3x3 submatrix
    det =   src[0][0]*src[1][1]*src[2][2] + src[0][1]*src[1][2]*src[2][0] + src[0][2]*src[1][0]*src[2][1]
          - src[2][0]*src[1][1]*src[0][2] - src[1][0]*src[0][1]*src[2][2] - src[0][0]*src[2][1]*src[1][2];


    // check if matrix is singular
    if(det==0.0f)return 0;


    // compute the inverse of the upper submatrix:

    // find the transposed matrix of cofactors of the upper submatrix
    // and multiply by (1/det)

    det = 1.0f / det;


    m[0][0] =  (src[1][1]*src[2][2] - src[2][1]*src[1][2]) * det;
    m[0][1] = -(src[0][1]*src[2][2] - src[2][1]*src[0][2]) * det;
    m[0][2] =  (src[0][1]*src[1][2] - src[1][1]*src[0][2]) * det;

    m[1][0] = -(src[1][0]*src[2][2] - src[2][0]*src[1][2]) * det;
    m[1][1] =  (src[0][0]*src[2][2] - src[2][0]*src[0][2]) * det;
    m[1][2] = -(src[0][0]*src[1][2] - src[1][0]*src[0][2]) * det;

    m[2][0] =  (src[1][0]*src[2][1] - src[2][0]*src[1][1]) * det;
    m[2][1] = -(src[0][0]*src[2][1] - src[2][0]*src[0][1]) * det;
    m[2][2] =  (src[0][0]*src[1][1] - src[1][0]*src[0][1]) * det;


    // compute (invA)*(-C)
    m[0][3] = -m[0][0]*src[0][3] - m[0][1]*src[1][3] - m[0][2]*src[2][3];
    m[1][3] = -m[1][0]*src[0][3] - m[1][1]*src[1][3] - m[1][2]*src[2][3];
    m[2][3] = -m[2][0]*src[0][3] - m[2][1]*src[1][3] - m[2][2]*src[2][3];

    // copy back if needed
    if( m == mTmp )
        c_guMtxCopy(mTmp,inv);

    return 1;
}

void c_guMtxTranspose(Mtx src,Mtx xPose)
{
    Mtx mTmp;
    MtxP m;

    if(src==xPose)
        m = mTmp;
    else
        m = xPose;


    m[0][0] = src[0][0];   m[0][1] = src[1][0];      m[0][2] = src[2][0];     m[0][3] = 0.0f;
    m[1][0] = src[0][1];   m[1][1] = src[1][1];      m[1][2] = src[2][1];     m[1][3] = 0.0f;
    m[2][0] = src[0][2];   m[2][1] = src[1][2];      m[2][2] = src[2][2];     m[2][3] = 0.0f;


    // copy back if needed
    if(m==mTmp)
        c_guMtxCopy(mTmp,xPose);
}

void c_guMtxReflect(Mtx m,Vector *p,Vector *n)
{
    f32 vxy, vxz, vyz, pdotn;

    vxy   = -2.0f * n->x * n->y;
    vxz   = -2.0f * n->x * n->z;
    vyz   = -2.0f * n->y * n->z;
    pdotn = 2.0f * c_guVecDotProduct(p,n);

    m[0][0] = 1.0f - 2.0f * n->x * n->x;
    m[0][1] = vxy;
    m[0][2] = vxz;
    m[0][3] = pdotn * n->x;

    m[1][0] = vxy;
    m[1][1] = 1.0f - 2.0f * n->y * n->y;
    m[1][2] = vyz;
    m[1][3] = pdotn * n->y;

    m[2][0] = vxz;
    m[2][1] = vyz;
    m[2][2] = 1.0f - 2.0f * n->z * n->z;
    m[2][3] = pdotn * n->z;
}


void c_guVecAdd(Vector *a,Vector *b,Vector *ab)
{
    ab->x = a->x + b->x;
    ab->y = a->y + b->y;
    ab->z = a->z + b->z;
}

void c_guVecSub(Vector *a,Vector *b,Vector *ab)
{
    ab->x = a->x - b->x;
    ab->y = a->y - b->y;
    ab->z = a->z - b->z;
}

void c_guVecScale(Vector *src,Vector *dst,f32 scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}


void c_guVecNormalize(Vector *v)
{
	f32 m;

	m = ((v->x)*(v->x)) + ((v->y)*(v->y)) + ((v->z)*(v->z));
	m = 1/sqrtf(m);
	v->x *= m;
	v->y *= m;
	v->z *= m;
}

void c_guVecCross(Vector *a,Vector *b,Vector *axb)
{
	Vector vTmp;

	vTmp.x = (a->y*b->z)-(a->z*b->y);
	vTmp.y = (a->z*b->x)-(a->x*b->z);
	vTmp.z = (a->x*b->y)-(a->y*b->x);

	axb->x = vTmp.x;
	axb->y = vTmp.y;
	axb->z = vTmp.z;
}

void c_guVecMultiply(Mtx mt,Vector *src,Vector *dst)
{
	Vector tmp;
	
    tmp.x = mt[0][0]*src->x + mt[0][1]*src->y + mt[0][2]*src->z + mt[0][3];
    tmp.y = mt[1][0]*src->x + mt[1][1]*src->y + mt[1][2]*src->z + mt[1][3];
    tmp.z = mt[2][0]*src->x + mt[2][1]*src->y + mt[2][2]*src->z + mt[2][3];

    dst->x = tmp.x;
    dst->y = tmp.y;
    dst->z = tmp.z;
}

void c_guVecMultiplySR(Mtx mt,Vector *src,Vector *dst)
{
	Vector tmp;
	
    tmp.x = mt[0][0]*src->x + mt[0][1]*src->y + mt[0][2]*src->z;
    tmp.y = mt[1][0]*src->x + mt[1][1]*src->y + mt[1][2]*src->z;
    tmp.z = mt[2][0]*src->x + mt[2][1]*src->y + mt[2][2]*src->z;

    // copy back
    dst->x = tmp.x;
    dst->y = tmp.y;
    dst->z = tmp.z;
}

f32 c_guVecDotProduct(Vector *a,Vector *b)
{
    f32 dot;

	dot = (a->x * b->x) + (a->y * b->y) + (a->z * b->z);

    return dot;
}

void c_guQuatAdd(Quaternion *a,Quaternion *b,Quaternion *ab)
{
	ab->x = a->x + b->x;
	ab->y = a->x + b->y;
	ab->z = a->x + b->z;
	ab->w = a->x + b->w;
}

void c_guQuatSub(Quaternion *a,Quaternion *b,Quaternion *ab)
{
	ab->x = a->x - b->x;
	ab->y = a->x - b->y;
	ab->z = a->x - b->z;
	ab->w = a->x - b->w;
}

void c_guQuatMultiply(Quaternion *a,Quaternion *b,Quaternion *ab)
{
	Quaternion *r;
	Quaternion ab_tmp;
	
	if(a==ab || b==ab) r = &ab_tmp;
	else r = ab;
	
	r->w = a->w*b->w - a->x*b->x - a->y*b->y - a->z*b->z;
	r->x = a->w*b->x + a->x*b->w + a->y*b->z + a->z*b->y;
	r->y = a->w*b->y + a->y*b->w + a->z*b->x + a->x*b->z;
	r->z = a->w*b->z + a->z*b->w + a->x*b->y + a->y*b->x;

	if(r==&ab_tmp) *ab = ab_tmp;
}

void guVecHalfAngle(Vector *a,Vector *b,Vector *half)
{
	Vector tmp1,tmp2,tmp3;

	tmp1.x = -a->x;
	tmp1.y = -a->y;
	tmp1.z = -a->z;

	tmp2.x = -b->x;
	tmp2.y = -b->y;
	tmp2.z = -b->z;

	guVecNormalize(&tmp1);
	guVecNormalize(&tmp2);

	guVecAdd(&tmp1,&tmp2,&tmp3);
	if(guVecDotProduct(&tmp3,&tmp3)>0.0f) guVecNormalize(&tmp3);

	*half = tmp3;
}
