/*
 * GLSLShader.cpp
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

#include "vislib/GLSLShader.h"

#include "vislib/Array.h"
#include "the/argument_exception.h"
#include "the/invalid_operation_exception.h"
#include "the/memory.h"
#include "vislib/RawStorage.h"
#include "vislib/sysfunctions.h"
#include "the/not_supported_exception.h"
#include "the/string.h"
#include "the/text/string_builder.h"
#include "the/text/string_buffer.h"


/*
 * vislib::graphics::gl::GLSLShader::IsValidHandle
 */
bool vislib::graphics::gl::GLSLShader::IsValidHandle(GLhandleARB hProg) {
    USES_GL_VERIFY;
    GLint status;

    if ((glGetObjectParameterivARB != NULL)
            && GL_SUCCEEDED(::glGetObjectParameterivARB(hProg, 
                GL_OBJECT_DELETE_STATUS_ARB, &status))) {
        return (status == 0);
    } else {
        return false;
    }
}


/*
 * vislib::graphics::gl::GLSLShader::RequiredExtensions
 */
const char * vislib::graphics::gl::GLSLShader::RequiredExtensions(void) {
    return "GL_ARB_shader_objects GL_ARB_vertex_shader GL_ARB_vertex_program ";
}


/*
 * vislib::graphics::gl::GLSLShader::FTRANSFORM_VERTEX_SHADER_SRC
 */
const char *vislib::graphics::gl::GLSLShader::FTRANSFORM_VERTEX_SHADER_SRC =
    "void main() { "
    "    gl_Position = ftransform();"
    "}";


/*
 * vislib::graphics::gl::GLSLShader::GLSLShader
 */
vislib::graphics::gl::GLSLShader::GLSLShader(void) 
        : AbstractOpenGLShader(), hProgObj(0) {
}


/*
 * vislib::graphics::gl::GLSLShader::~GLSLShader
 */
vislib::graphics::gl::GLSLShader::~GLSLShader(void) {
    this->Release();
}


/*
 * vislib::graphics::gl::GLSLShader::BindAttribute
 */
GLenum vislib::graphics::gl::GLSLShader::BindAttribute(GLint index, 
        const char *name) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    GL_VERIFY_RETURN(::glBindAttribLocationARB(this->hProgObj, index, name));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::Compile
 */
bool vislib::graphics::gl::GLSLShader::Compile(const char *vertexShaderSrc, 
                                              const char *fragmentShaderSrc) {
    const char *v[] = { vertexShaderSrc };
    const char *f[] = { fragmentShaderSrc };
    
    return this->Compile(v, 1, f, 1, false);
}


/*
 * vislib::graphics::gl::GLSLShader::Compile
 */
bool vislib::graphics::gl::GLSLShader::Compile(const char **vertexShaderSrc, 
        const size_t cntVertexShaderSrc, const char **fragmentShaderSrc,
        const size_t cntFragmentShaderSrc, bool insertLineDirective) {
    USES_GL_VERIFY;
    THE_ASSERT(vertexShaderSrc != NULL);
    THE_ASSERT(fragmentShaderSrc != NULL);

    this->Release();

    GLhandleARB hPixelShader = this->compileNewShader(GL_FRAGMENT_SHADER_ARB,
        fragmentShaderSrc, static_cast<GLsizei>(cntFragmentShaderSrc), 
        insertLineDirective);
    GLhandleARB hVertexShader = this->compileNewShader(GL_VERTEX_SHADER_ARB,
        vertexShaderSrc, static_cast<GLsizei>(cntVertexShaderSrc), 
        insertLineDirective);

    /* Assemble program object. */
    GL_VERIFY_THROW(this->hProgObj = ::glCreateProgramObjectARB());
    GL_VERIFY_THROW(::glAttachObjectARB(this->hProgObj, hVertexShader));
    GL_VERIFY_THROW(::glAttachObjectARB(this->hProgObj, hPixelShader));

    return true;
}


/*
 * vislib::graphics::gl::GLSLShader::CompileFromFile
 */
bool vislib::graphics::gl::GLSLShader::CompileFromFile(
        const char *vertexShaderFile, const char *fragmentShaderFile) {
    the::astring vertexShaderSrc;
    the::astring fragmentShaderSrc;

    if (!vislib::sys::ReadTextFile(vertexShaderSrc, vertexShaderFile)) {
        return false;
    }

    if (!vislib::sys::ReadTextFile(fragmentShaderSrc, fragmentShaderFile)) {
        return false;
    }

    return this->Compile(vertexShaderSrc.c_str(), fragmentShaderSrc.c_str());
}


/*
 * vislib::graphics::gl::GLSLShader::CompileFromFile
 */
bool vislib::graphics::gl::GLSLShader::CompileFromFile(
        const char **vertexShaderFiles, const size_t cntVertexShaderFiles, 
        const char **fragmentShaderFiles, 
        const size_t cntFragmentShaderFiles, bool insertLineDirective) {

    // using arrays for automatic cleanup when a 'read' throws an exception
    Array<the::astring> vertexShaderSrcs(cntVertexShaderFiles);
    Array<the::astring> fragmentShaderSrcs(cntFragmentShaderFiles);

    for(size_t i = 0; i < cntVertexShaderFiles; i++) {
        if (!vislib::sys::ReadTextFile(vertexShaderSrcs[i], 
                vertexShaderFiles[i])) {
            return false;
        }
    }

    for(size_t i = 0; i < cntFragmentShaderFiles; i++) {
        if (!vislib::sys::ReadTextFile(fragmentShaderSrcs[i], 
                fragmentShaderFiles[i])) {
            return false;
        }
    }

    // built up pointer arrays for attributes
    const char **vertexShaderSrcPtrs = new const char*[cntVertexShaderFiles];
    const char **fragmentShaderSrcPtrs 
        = new const char*[cntFragmentShaderFiles];

    try {
        for(size_t i = 0; i < cntVertexShaderFiles; i++) {
            vertexShaderSrcPtrs[i] = vertexShaderSrcs[i].c_str();
        }
        for(size_t i = 0; i < cntFragmentShaderFiles; i++) {
            fragmentShaderSrcPtrs[i] = fragmentShaderSrcs[i].c_str();
        }

        bool retval = this->Compile(vertexShaderSrcPtrs, cntVertexShaderFiles, 
            fragmentShaderSrcPtrs, cntFragmentShaderFiles, 
            insertLineDirective);

        the::safe_array_delete(vertexShaderSrcPtrs);
        the::safe_array_delete(fragmentShaderSrcPtrs);

        return retval;

        // free pointer arrays on exception
    } catch(OpenGLException e) { // catch OpenGLException to avoid truncating
        the::safe_array_delete(vertexShaderSrcPtrs);
        the::safe_array_delete(fragmentShaderSrcPtrs);
        throw e;
    } catch(CompileException e) {
        the::safe_array_delete(vertexShaderSrcPtrs);
        the::safe_array_delete(fragmentShaderSrcPtrs);
        throw e;
    } catch(the::exception e) {
        the::safe_array_delete(vertexShaderSrcPtrs);
        the::safe_array_delete(fragmentShaderSrcPtrs);
        throw e;
    } catch(...) {
        the::safe_array_delete(vertexShaderSrcPtrs);
        the::safe_array_delete(fragmentShaderSrcPtrs);
        throw the::exception("Unknown Exception", __FILE__, __LINE__);
    }

    return false; // should be unreachable code!
}


/*
 * vislib::graphics::gl::GLSLShader::Create
 */
bool vislib::graphics::gl::GLSLShader::Create(const char *vertexShaderSrc, 
                                              const char *fragmentShaderSrc) {
    if (this->Compile(vertexShaderSrc, fragmentShaderSrc)) {
        return this->Link();
    } else {
        return false;
    }
}


/*
 * vislib::graphics::gl::GLSLShader::Create
 */
bool vislib::graphics::gl::GLSLShader::Create(const char **vertexShaderSrc, 
        const size_t cntVertexShaderSrc, const char **fragmentShaderSrc,
        const size_t cntFragmentShaderSrc, bool insertLineDirective) {
    if (this->Compile(vertexShaderSrc, cntVertexShaderSrc, fragmentShaderSrc, 
            cntFragmentShaderSrc, insertLineDirective)) {
        return this->Link();
    } else {
        return false;
    }
}


/*
 * vislib::graphics::gl::GLSLShader::CreateFromFile
 */
bool vislib::graphics::gl::GLSLShader::CreateFromFile(
        const char *vertexShaderFile, const char *fragmentShaderFile) {
    if (this->CompileFromFile(vertexShaderFile, fragmentShaderFile)) {
        return this->Link();
    } else {
        return false;
    }
}


/*
 * vislib::graphics::gl::GLSLShader::CreateFromFile
 */
bool vislib::graphics::gl::GLSLShader::CreateFromFile(
        const char **vertexShaderFiles, const size_t cntVertexShaderFiles, 
        const char **fragmentShaderFiles, 
        const size_t cntFragmentShaderFiles, bool insertLineDirective) {

    if (this->CompileFromFile(vertexShaderFiles, cntVertexShaderFiles, 
            fragmentShaderFiles, cntFragmentShaderFiles, 
            insertLineDirective)) {
        return this->Link();
    } else {
        return false;
    }
}


/*
 * vislib::graphics::gl::GLSLShader::Disable
 */
GLenum vislib::graphics::gl::GLSLShader::Disable(void) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    GL_VERIFY_RETURN(::glUseProgramObjectARB(0));
    GL_VERIFY_RETURN(::glDisable(GL_VERTEX_PROGRAM_ARB));
    GL_VERIFY_RETURN(::glDisable(GL_FRAGMENT_PROGRAM_ARB));
    return GL_NO_ERROR;
}
        

/*
 * vislib::graphics::gl::GLSLShader::Enable
 */
GLenum vislib::graphics::gl::GLSLShader::Enable(void) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    GL_VERIFY_RETURN(::glEnable(GL_VERTEX_PROGRAM_ARB));
    GL_VERIFY_RETURN(::glEnable(GL_FRAGMENT_PROGRAM_ARB));
    GL_VERIFY_RETURN(::glUseProgramObjectARB(this->hProgObj));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::Link
 */
bool vislib::graphics::gl::GLSLShader::Link() {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));
    
    GL_VERIFY_THROW(::glLinkProgramARB(this->hProgObj));
    if (!this->isLinked(this->hProgObj)) {
        throw CompileException(this->getProgramInfoLog(this->hProgObj).c_str(), 
            CompileException::ACTION_LINK, __FILE__, __LINE__);
    }

    return true;
}


/*
 * vislib::graphics::gl::GLSLShader::ParameterLocation
 */
GLint vislib::graphics::gl::GLSLShader::ParameterLocation(const char *name) const {
    THE_ASSERT(name != NULL);
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));
    return ::glGetUniformLocationARB(this->hProgObj, name);
}


/*
 * vislib::graphics::gl::GLSLShader::Release
 */
GLenum vislib::graphics::gl::GLSLShader::Release(void) {
    USES_GL_VERIFY;

    if (GLSLShader::IsValidHandle(this->hProgObj)) {
        GLint objCnt;

        if (GL_SUCCEEDED(::glGetObjectParameterivARB(this->hProgObj, 
                GL_OBJECT_ATTACHED_OBJECTS_ARB, &objCnt))) {
            GLhandleARB *objs = new GLhandleARB[objCnt];

            if (GL_SUCCEEDED(::glGetAttachedObjectsARB(this->hProgObj, 
                    objCnt, &objCnt, objs))) {
                for (GLint i = 0; i < objCnt; i++) {
                    ::glDeleteObjectARB(objs[i]);
                }
            }
            delete[] objs;
        }

        GL_VERIFY_RETURN(::glDeleteObjectARB(this->hProgObj));
    }

    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name,
        const float v1) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform1fARB(name, v1));
    return GL_NO_ERROR;

}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name, 
        const float v1, const float v2) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform2fARB(name, v1, v2));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name, 
        const float v1, const float v2, const float v3) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform3fARB(name, v1, v2, v3));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name, 
        const float v1, const float v2, const float v3, const float v4) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform4fARB(name, v1, v2, v3, v4));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name, 
        const int v1) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform1iARB(name, v1));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name, 
        const int v1, const int v2) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform2iARB(name, v1, v2));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name, 
        const int v1, const int v2, const int v3) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform3iARB(name, v1, v2, v3));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameter
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameter(const GLint name, 
        const int v1, const int v2, const int v3, const int v4) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform4iARB(name, v1, v2, v3, v4));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray1
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray1(const GLint name,
        const GLsizei count, const float *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform1fvARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray2
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray2(const GLint name,
        const GLsizei count, const float *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform2fvARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray3
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray3(const GLint name,
        const GLsizei count, const float *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform3fvARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray4
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray4(const GLint name,
        const GLsizei count, const float *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform4fvARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray1
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray1(const GLint name,
        const GLsizei count, const int *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform1ivARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray2
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray2(const GLint name,
        const GLsizei count, const int *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform2ivARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray3
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray3(const GLint name,
        const GLsizei count, const int *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform3ivARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::SetParameterArray4
 */
GLenum vislib::graphics::gl::GLSLShader::SetParameterArray4(const GLint name,
        const GLsizei count, const int *value) {
    USES_GL_VERIFY;
    THE_ASSERT(GLSLShader::IsValidHandle(this->hProgObj));

    if (name < 0) {
        return GL_INVALID_VALUE;
    }
    GL_VERIFY_RETURN(::glUniform4ivARB(name, count, value));
    return GL_NO_ERROR;
}


/*
 * vislib::graphics::gl::GLSLShader::compileNewShader
 */
GLhandleARB vislib::graphics::gl::GLSLShader::compileNewShader(GLenum type, 
        const char **src, GLsizei cnt, bool insertLineDirective) {
    USES_GL_VERIFY;
    GLhandleARB shader;
    RawStorage powerMemory;
    const char lineStr[] = "\n#line 0 %d\n";

    if (insertLineDirective && (cnt > 1)) {
        the::astring tmp;
        char *ptr;
        the::text::astring_builder::format_to(tmp, lineStr, cnt);

        // very tricky:
        powerMemory.AssertSize((sizeof(char*) * (cnt * 2 - 1)) 
            + ((cnt - 1) * sizeof(char) * (tmp.size() + 1)));
        ptr = powerMemory.As<char>() + (sizeof(char*) * (cnt = cnt * 2 - 1));
        for (GLsizei i = 0; i < cnt; i++) {
            if (i % 2 == 0) {
                powerMemory.As<const char*>()[i] = src[i / 2];
            } else {
                unsigned int len;
                the::text::astring_builder::format_to(tmp, lineStr, int((i + 1) / 2));
                len = static_cast<unsigned int>((tmp.size() + 1) * sizeof(char));
                memcpy(ptr, tmp.c_str(), len);
                powerMemory.As<char*>()[i] = ptr;
                ptr += len;
            }
        }

        src = powerMemory.As<const char*>();
    }

    GL_VERIFY_THROW(shader = ::glCreateShaderObjectARB(type));
    GL_VERIFY_THROW(::glShaderSourceARB(shader, cnt, src, NULL));
    GL_VERIFY_THROW(::glCompileShaderARB(shader));

    if (!isCompiled(shader)) {
        throw CompileException(getProgramInfoLog(shader).c_str(), 
            CompileException::CompilationFailedAction(type), 
            __FILE__, __LINE__);
    }

    return shader;
}


/*
 * vislib::graphics::gl::GLSLShader::getProgramInfoLog
 */
the::astring vislib::graphics::gl::GLSLShader::getProgramInfoLog(
        GLhandleARB hProg) {
    USES_GL_VERIFY;
    GLint len = 0;
    GLint written = 0;
    the::astring retval;

    GL_VERIFY_THROW(::glGetObjectParameterivARB(hProg, 
        GL_OBJECT_INFO_LOG_LENGTH_ARB, &len));

    if (len > 0) {
        GL_VERIFY_THROW(::glGetInfoLogARB(hProg, len, &written,
            the::text::string_buffer_allocate(retval, len)));
    }

    return retval;
}


/*
 * vislib::graphics::gl::GLSLShader::isCompiled
 */
bool vislib::graphics::gl::GLSLShader::isCompiled(GLhandleARB hProg) {
    USES_GL_VERIFY;
    GLint status;

    GL_VERIFY_THROW(::glGetObjectParameterivARB(hProg, 
        GL_OBJECT_COMPILE_STATUS_ARB, &status));

    return (status != GL_FALSE);
}


/*
 * vislib::graphics::gl::GLSLShader::isLinked
 */
bool vislib::graphics::gl::GLSLShader::isLinked(GLhandleARB hProg) {
    USES_GL_VERIFY;
    GLint status;

    GL_VERIFY_THROW(::glGetObjectParameterivARB(hProg, 
        GL_OBJECT_LINK_STATUS_ARB, &status));

    return (status != GL_FALSE);
}
