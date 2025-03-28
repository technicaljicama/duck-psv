
int powf_sfp(int a1, int a2)
{
	float fa1, fa2;
	int ires;

	fa1 = *(float *)(&a1);
	fa2 = *(float *)(&a2);
	float fres = powf(fa1, fa2);
	ires = *(int *)(&fres);

	return ires;
}

int64_t acos_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = acos(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t asin_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = asin(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t atan_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = atan(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t atan2_sfp(int64_t a1, int64_t a2)
{
	double fa1, fa2;
	int64_t ires;

	fa1 = *(double *)(&a1);
	fa2 = *(double *)(&a2);
	double fres = atan2(fa1, fa2);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t ceil_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = ceil(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t cos_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = cos(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t floor_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = floor(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t fmod_sfp(int64_t a1, int64_t a2)
{
	double fa1, fa2;
	int64_t ires;

	fa1 = *(double *)(&a1);
	fa2 = *(double *)(&a2);
	double fres = fmod(fa1, fa2);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t ldexp_sfp(int64_t a1, int a2)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = ldexp(fa1, a2);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t log_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = log(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t pow_sfp(int64_t a1, int64_t a2)
{
	double fa1, fa2;
	int64_t ires;

	fa1 = *(double *)(&a1);
	fa2 = *(double *)(&a2);
	double fres = pow(fa1, fa2);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t sin_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = sin(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t sqrt_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = sqrt(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}

int64_t tan_sfp(int64_t a1)
{
	double fa1;
	int64_t ires;

	fa1 = *(double *)(&a1);
	double fres = tan(fa1);
	ires = *(int64_t *)(&fres);

	return ires;
}
void glUniform1f_sfp(GLint location, int x)
{
    float fa1;
    fa1 = *(float *)(&x);
    glUniform1f(location, fa1);
}

void glUniform2f_sfp(GLint location, int x, int y)
{
    float fa1, fa2;
    fa1 = *(float *)(&x);
    fa2 = *(float *)(&y);
    glUniform2f(location, fa1, fa2);
}

void glUniform3f_sfp(GLint location, int x, int y, int z)
{
    float fa1, fa2, fa3;
    fa1 = *(float *)(&x);
    fa2 = *(float *)(&y);
    fa3 = *(float *)(&z);
    glUniform3f(location, fa1, fa2, fa3);
}

void glUniform4f_sfp(GLint location, int x, int y, int z, int w)
{
    float fa1, fa2, fa3, fa4;
    fa1 = *(float *)(&x);
    fa2 = *(float *)(&y);
    fa3 = *(float *)(&z);
    fa4 = *(float *)(&w);
    glUniform4f(location, fa1, fa2, fa3, fa4);
}

void glUniform1fv_sfp(GLint location, GLsizei count, const int *value)
{
    float *fv;
    fv = (float *)value;
    glUniform1fv(location, count, fv);
}

void glUniform2fv_sfp(GLint location, GLsizei count, const int *value)
{
    float *fv;
    fv = (float *)value;
    glUniform2fv(location, count, fv);
}

void glUniform3fv_sfp(GLint location, GLsizei count, const int *value)
{
    float *fv;
    fv = (float *)value;
    glUniform3fv(location, count, fv);
}

void glUniform4fv_sfp(GLint location, GLsizei count, const int *value)
{
    float *fv;
    fv = (float *)value;
    glUniform4fv(location, count, fv);
}

void glGetFloatv_sfp(GLenum pname, int *params)
{
    float *fp;
    fp = (float *)params;
    glGetFloatv(pname, fp);
}
void glClearColor_sfp(int red, int green, int blue, int alpha)
{
    float fr, fg, fb, fa;
    fr = *(float *)(&red);
    fg = *(float *)(&green);
    fb = *(float *)(&blue);
    fa = *(float *)(&alpha);
	// printf("glClearColor %f, %f, %f\n", fr, fg, fb);
    glClearColor(fr, fg, fb, fa);
}
void glUniformMatrix4fv_sfp(GLint location, GLsizei count, GLboolean transpose, const int *value)
{
    float *fv;
    fv = (float *)value;
    glUniformMatrix4fv(location, count, transpose, fv);
}
