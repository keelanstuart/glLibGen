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

// GLlibtestView.h : interface of the CGLlibtestView class
//

#pragma once

#include "gl_wrapper.h"

class CGLlibtestView : public CView
{
protected: // create from serialization only
	CGLlibtestView();
	DECLARE_DYNCREATE(CGLlibtestView)

// Attributes
public:
	CGLlibtestDoc* GetDocument() const;

	COpenGL gl;
	HGLRC mGLRC;

// Operations
public:

// Overrides
public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:

// Implementation
public:
	virtual ~CGLlibtestView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	afx_msg void OnFilePrintPreview();
	void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	DECLARE_MESSAGE_MAP()
public:
	virtual void OnInitialUpdate();
};

#ifndef _DEBUG  // debug version in GLlibtestView.cpp
inline CGLlibtestDoc* CGLlibtestView::GetDocument() const
   { return reinterpret_cast<CGLlibtestDoc*>(m_pDocument); }
#endif

