//
//  main.cpp
//  LearnOpenGL
//
//  Created by changbo on 2019/10/12.
//  Copyright © 2019 CB. All rights reserved.
//

#include "GLTools.h"
#include "GLShaderManager.h"
#include "GLFrustum.h"
#include "GLBatch.h"
#include "GLMatrixStack.h"
#include "GLGeometryTransform.h"
#include "StopWatch.h"

#include <math.h>
#include <stdio.h>

#ifdef __APPLE__
#include <glut/glut.h>
#else
#define FREEGLUT_STATIC
#include <GL/glut.h>
#endif


// 添加随机小球
#define NUM_SPHERES 50
GLFrame spheres[NUM_SPHERES];

// 着色器管理器
GLShaderManager        shaderManager;

// 模型视图矩阵
GLMatrixStack          modelViewMatrix;

// 投影矩阵
GLMatrixStack          projectionMatrix;

// 视景体
GLFrustum              viewFrustum;

// 几何图形变换管道
GLGeometryTransform    transformPipeline;

// 花托批处理
GLTriangleBatch        torusBatch;

// 地板批处理
GLBatch                floorBatch;

//** 定义公转球的批处理（公转自转）**
//球批处理
GLTriangleBatch        sphereBatch;

//** 角色帧 照相机角色帧（全局照相机实例）
GLFrame                cameraFrame;

// 添加纹理标记数组
GLuint uiTextures[3];

bool LoadTGATexture(const char *szFileName, GLenum minFilter, GLenum magFilter, GLenum wrapMode) {
    
    GLbyte *pBits;
    int nWidth,nHeight,nComponets;
    GLenum eFormat;
    
    pBits = gltReadTGABits(szFileName, &nWidth, &nHeight, &nComponets, &eFormat);
    if (pBits == NULL) {
        return false;
    }
    
    
    // 设置纹理参数
    // 环绕方式
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    
    // 过滤方式
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    
    // 包装一下
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    // 载入纹理
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB, nWidth, nHeight, 0, eFormat, GL_UNSIGNED_BYTE, pBits);
    
    free(pBits);
    
    
    //只有minFilter 等于以下四种模式，才可以生成Mip贴图
    // GL_NEAREST_MIPMAP_NEAREST 具有非常好的性能，并且闪烁现象非常弱
    // GL_LINEAR_MIPMAP_NEAREST 常常用于对游戏进行加速，它使用了高质量的线性过滤器
    // GL_LINEAR_MIPMAP_LINEAR 和 GL_NEAREST_MIPMAP_LINEAR 过滤器在Mip层之间执行了一些额外的插值，以消除他们之间的过滤痕迹。
    // GL_LINEAR_MIPMAP_LINEAR 三线性Mip贴图。纹理过滤的黄金准则，具有最高的精度。
    if(minFilter == GL_LINEAR_MIPMAP_LINEAR || minFilter == GL_LINEAR_MIPMAP_NEAREST ||
       minFilter == GL_NEAREST_MIPMAP_LINEAR || minFilter == GL_NEAREST_MIPMAP_NEAREST) {
        //加载Mip,纹理生成所有的Mip层
        //参数：GL_TEXTURE_1D、GL_TEXTURE_2D、GL_TEXTURE_3D
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    
    
    return true;
}


// 为程序作一次性的设置
void SetupRC()
{
    // 初始化着色器管理器
    shaderManager.InitializeStockShaders();
    
    // 开启深度测试
    glEnable(GL_DEPTH_TEST);
    // 开启正背面剔除
    glEnable(GL_CULL_FACE);
    
    // 设置清屏颜色到颜色缓存区
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    // 绘制游泳圈
    gltMakeTorus(torusBatch, 0.4f, 0.15f, 30, 30);
    
    // 球体（公转自转）
    gltMakeSphere(sphereBatch, 0.1f, 26, 13);
    
    
    // 对地板批次添加纹理顶点数据
    // 加载纹理
    
    GLfloat texSize = 10.0f;
    floorBatch.Begin(GL_TRIANGLE_FAN, 4, 1);
    floorBatch.MultiTexCoord2f(0, 0, 0);
    floorBatch.Vertex3f(-20.0f, -0.41f, 20.0f);
    
    floorBatch.MultiTexCoord2f(0, texSize, 0);
    floorBatch.Vertex3f(20.0f, -0.41f, -20.0f);
    
    floorBatch.MultiTexCoord2f(0, texSize, texSize);
    floorBatch.Vertex3f(20.0f, -0.41f, -20.0f);
    
    floorBatch.MultiTexCoord2f(0, 0, texSize);
    floorBatch.Vertex3f(-20.0f, -0.41f, -20.0f);
    
    floorBatch.End();
    
    
    // 绑定纹理
    glGenTextures(3, uiTextures);
    glBindTexture(GL_TEXTURE_2D, uiTextures[0]);
    LoadTGATexture("marble.tga", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, uiTextures[1]);
    LoadTGATexture("marslike.tga", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
    LoadTGATexture("moonlike.tga", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
    
    
    // 在场景中随机位置，初始化许多的小球体
    for (int i=0; i<NUM_SPHERES; i++) {
        //y轴不变，x，z随机值
        GLfloat x = ((GLfloat)((rand()%400)-200)*0.1f);
        GLfloat z = ((GLfloat)((rand()%400)-200)*0.1f);
        
        // 设置球体y方向为0.0，可以看起来漂浮在眼睛的高度
        // 对spheres数组中每一个顶点，设置顶点数据
        spheres[i].SetOrigin(x, 0.0f, z);
        
    }
}


//窗口大小改变时接受新的宽度和高度，其中0,0代表窗口中视口的左下角坐标，w，h代表像素
void ChangeSize(int w,int h)
{
    glViewport(0,0, w, h);
    
    // 创建投影矩阵
    viewFrustum.SetPerspective(45.0f, float(w)/float(h), 1.0f, 100.0f);
    
    // 加载到投影矩阵堆栈上
    projectionMatrix.LoadMatrix(viewFrustum.GetProjectionMatrix());
    
    // 设置变换管道以使用两个矩阵堆栈（变换矩阵modelViewMatrix ，投影矩阵projectionMatrix）
    // 初始化GLGeometryTransform 的实例transformPipeline.通过将它的内部指针设置为模型视图矩阵堆栈 和 投影矩阵堆栈实例，来完成初始化
    // 当然这个操作也可以在SetupRC 函数中完成，但是在窗口大小改变时或者窗口创建时设置它们并没有坏处。而且这样可以一次性完成矩阵和管线的设置。
    transformPipeline.SetMatrixStacks(modelViewMatrix, projectionMatrix);
}

void DrawSongAndDance(GLfloat yRot) {
    
    static GLfloat vWhite[] = {1.0f, 1.0f, 1.0f, 1.0f};
    static GLfloat vLightPos[] = {0.0f, 3.0f, 0.0f, 1.0f};
    
    //  添加光源
    
    M3DVector4f vLightTransformed;
    
    // 定义一个4*4的相机矩阵
    M3DMatrix44f mCamera;
    // 从 modelViewMatrix 获取矩阵堆栈顶部的值
    modelViewMatrix.GetMatrix(mCamera);
    // 将照相机矩阵 mCamera 与 光源矩阵 vLightPos 相乘获得 vLightTransformed 矩阵
    m3dTransformVector4(vLightTransformed, vLightPos, mCamera);
    
    // 将结果压栈
    modelViewMatrix.PushMatrix();
    // 仿射变换，平移
    modelViewMatrix.Translatev(vLightPos);
    
    // 绘制 （平面着色器）
    shaderManager.UseStockShader(GLT_SHADER_FLAT,
                                 transformPipeline.GetModelViewProjectionMatrix(),
                                 vWhite);
    sphereBatch.Draw();
    
    // 恢复矩阵
    modelViewMatrix.PopMatrix();
    
    // 绘制悬浮球体
    glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
    for (int i=0; i<NUM_SPHERES; i++) {
        modelViewMatrix.PushMatrix();
        modelViewMatrix.MultMatrix(spheres[i]);
        
        shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                     modelViewMatrix.GetMatrix(),
                                     transformPipeline.GetProjectionMatrix(),
                                     vLightTransformed, vWhite, 0);
        
        sphereBatch.Draw();
        
        modelViewMatrix.PopMatrix();
    }
    
    // 绘制游泳圈
    modelViewMatrix.Translate(0.0f, 0.2f, -2.5f); // 顶部矩阵沿着 z 轴移动2.5个单位
    // 保存平移
    modelViewMatrix.PushMatrix();
    // 顶部旋转yRot度
    modelViewMatrix.Rotate(yRot, 0.0f, 1.0f, 0.0f);
    // 绑定纹理
    glBindTexture(GL_TEXTURE_2D, uiTextures[1]);
    
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                 modelViewMatrix.GetMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightTransformed, vWhite, 0);
    
    torusBatch.Draw();
    
    modelViewMatrix.PopMatrix();
    modelViewMatrix.Rotate(yRot*-2.0f, 0.0f, 1.0f, 0.0f);
    modelViewMatrix.Translate(0.8f, 0.0f, 0.0f);
    
    glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
    
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF,
                                 modelViewMatrix.GetMatrix(),
                                 transformPipeline.GetProjectionMatrix(),
                                 vLightTransformed, vWhite, 0);
    
    sphereBatch.Draw();
}


//开始渲染 绘制场景
void RenderScene(void)
{
    // 地板颜色值
    static GLfloat vFloorColor[] = {1.0f, 1.0f, 0.0f, 0.75f};
    
    static CStopWatch rotTimer;
    float yRot = rotTimer.GetElapsedSeconds() * 60.0f;
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    modelViewMatrix.PushMatrix();
    
    // 观察者
    M3DMatrix44f    mCamera;
    cameraFrame.GetCameraMatrix(mCamera);
    modelViewMatrix.MultMatrix(mCamera);
    
    modelViewMatrix.PushMatrix();
    
    // 添加反光效果
    modelViewMatrix.Scale(1.0f, -1.0f, 1.0f); // 翻转 Y轴
    modelViewMatrix.Translate(0.0f, 0.8f, 0.0f); // 缩小一点
    
    glFrontFace(GL_CW);
    
    // 绘制地面以外的部分
    DrawSongAndDance(yRot);
    
    glFrontFace(GL_CCW);
    
    modelViewMatrix.PopMatrix();
    
    // 开启混合功能
    glEnable(GL_BLEND);
    // 绑定纹理
    glBindTexture(GL_TEXTURE_2D, uiTextures[0]);
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    shaderManager.UseStockShader(GLT_SHADER_TEXTURE_MODULATE,
                                 transformPipeline.GetModelViewProjectionMatrix(),
                                 vFloorColor, 0);
    floorBatch.Draw();
    
    //取消混合
    glDisable(GL_BLEND);
    // 绘制地面以上部分
    DrawSongAndDance(yRot);
    
    modelViewMatrix.PopMatrix();
    
    glutSwapBuffers();
    glutPostRedisplay();
}



// 移动照相机参考帧，来对方向键作出响应
void SpeacialKeys(int key,int x,int y)
{

    float linear = 0.2f;
    float angular = float(m3dDegToRad(4.0f));
    
    if (key == GLUT_KEY_UP) {
       
        //MoveForward 平移
        cameraFrame.MoveForward(linear);
    }
    
    if (key == GLUT_KEY_DOWN) {
        cameraFrame.MoveForward(-linear);
    }
    
    if (key == GLUT_KEY_LEFT) {
        //RotateWorld 旋转
        cameraFrame.RotateWorld(angular, 0.0f, 1.0f, 0.0f);
    }
    
    if (key == GLUT_KEY_RIGHT) {
        cameraFrame.RotateWorld(-angular, 0.0f, 1.0f, 0.0f);
    }
}


int main(int argc,char* argv[])
{
    //设置当前工作目录，针对MAC OS X
    gltSetWorkingDirectory(argv[0]);
    
    //初始化GLUT库
    glutInit(&argc, argv);
    
    /*初始化双缓冲窗口，其中标志GLUT_DOUBLE、GLUT_RGBA、GLUT_DEPTH、GLUT_STENCIL分别指
     双缓冲窗口、RGBA颜色模式、深度测试、模板缓冲区*/
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_STENCIL);
    
    //GLUT窗口大小，标题窗口
    glutInitWindowSize(800,600);
    
    glutCreateWindow("OpenGL SphereWorld");
    
    //注册回调函数
    glutReshapeFunc(ChangeSize);
    
    glutDisplayFunc(RenderScene);
    
    glutSpecialFunc(SpeacialKeys);
    
    //驱动程序的初始化中没有出现任何问题。
    GLenum err = glewInit();
    if(GLEW_OK != err) {
        fprintf(stderr,"glew error:%s\n",glewGetErrorString(err));
        return 1;
    }
    
    //调用SetupRC
    SetupRC();
    
    glutMainLoop();
    
    return 0;
}
