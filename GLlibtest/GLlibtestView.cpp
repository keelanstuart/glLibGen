// This MFC Samples source code demonstrates using MFC Microsoft Office Fluent User Interface 
// (the "Fluent UI") and is provided only as referential material to supplement the 
// Microsoft Foundation Classes Reference and related electronic documentation 
// included with the MFC C++ library software.  
// License terms to copy, use or distribute the Fluent UI are available separately.  
// To learn more about our Fluent UI licensing program, please visit 
// http://go.microsoft.com/fwlink/?LinkId=238214.
//
// Copyright (C) Microsoft Corporation
// All rights reserved.

// GLlibtestView.cpp : implementation of the CGLlibtestView class
//

#include "stdafx.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "GLlibtest.h"
#endif

#include "GLlibtestDoc.h"
#include "GLlibtestView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CGLlibtestView

IMPLEMENT_DYNCREATE(CGLlibtestView, CView)

BEGIN_MESSAGE_MAP(CGLlibtestView, CView)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// CGLlibtestView construction/destruction

CGLlibtestView::CGLlibtestView()
{
	mGLRC = NULL;
}

CGLlibtestView::~CGLlibtestView()
{
	if (mGLRC)
	{
		wglMakeCurrent(NULL, NULL);

		wglDeleteContext(mGLRC);
	}
}

BOOL CGLlibtestView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

// CGLlibtestView drawing

void CGLlibtestView::OnDraw(CDC *pDC)
{
	CGLlibtestDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	static bool initgl = false;

	HDC hdc = pDC->GetSafeHdc();

	if (!initgl)
	{
		initgl = true;

		PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd  
			1,                     // version number  
			PFD_DRAW_TO_WINDOW |   // support window  
			PFD_SUPPORT_OPENGL |   // support OpenGL  
			PFD_DOUBLEBUFFER,      // double buffered  
			PFD_TYPE_RGBA,         // RGBA type  
			24,                    // 24-bit color depth  
			0, 0, 0, 0, 0, 0,      // color bits ignored  
			0,                     // no alpha buffer  
			0,                     // shift bit ignored  
			0,                     // no accumulation buffer  
			0, 0, 0, 0,            // accum bits ignored  
			24,                    // 32-bit z-buffer  
			8,                     // no stencil buffer  
			0,                     // no auxiliary buffer  
			PFD_MAIN_PLANE,        // main layer  
			0,                     // reserved  
			0, 0, 0                // layer masks ignored  
		};

		int  iPixelFormat;

		// get the best available match of pixel format for the device context   
		iPixelFormat = ChoosePixelFormat(hdc, &pfd);

		// make that the pixel format of the device context  
		SetPixelFormat(hdc, iPixelFormat, &pfd);

		mGLRC = wglCreateContext(hdc);

		wglMakeCurrent(hdc, mGLRC);

		gl.Initialize();
	}

	CRect r;
	GetClientRect(r);
	gl.Viewport(r.left, r.top, r.Width(), r.Height());

	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();

	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();

	gl.ShadeModel(GL_SMOOTH);
	gl.ClearColor(1, 0, 1, 1);
	gl.ClearDepthf(1);
	gl.Enable(GL_DEPTH_TEST);
	gl.DepthFunc(GL_LEQUAL);
	gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl.Finish();

	SwapBuffers(hdc);
}

void CGLlibtestView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CGLlibtestView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CGLlibtestView diagnostics

#ifdef _DEBUG
void CGLlibtestView::AssertValid() const
{
	CView::AssertValid();
}

void CGLlibtestView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CGLlibtestDoc* CGLlibtestView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGLlibtestDoc)));
	return (CGLlibtestDoc*)m_pDocument;
}
#endif //_DEBUG


// CGLlibtestView message handlers


void CGLlibtestView::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	// TODO: Add your specialized code here and/or call the base class
}
