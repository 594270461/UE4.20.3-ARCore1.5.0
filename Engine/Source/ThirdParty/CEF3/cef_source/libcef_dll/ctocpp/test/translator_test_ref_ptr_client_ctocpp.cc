// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//

#include "libcef_dll/ctocpp/test/translator_test_ref_ptr_client_ctocpp.h"
#include "libcef_dll/ctocpp/test/translator_test_ref_ptr_client_child_ctocpp.h"


// VIRTUAL METHODS - Body may be edited by hand.

int CefTranslatorTestRefPtrClientCToCpp::GetValue() {
  cef_translator_test_ref_ptr_client_t* _struct = GetStruct();
  if (CEF_MEMBER_MISSING(_struct, get_value))
    return 0;

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  int _retval = _struct->get_value(_struct);

  // Return type: simple
  return _retval;
}


// CONSTRUCTOR - Do not edit by hand.

CefTranslatorTestRefPtrClientCToCpp::CefTranslatorTestRefPtrClientCToCpp() {
}

template<> cef_translator_test_ref_ptr_client_t* CefCToCppRefCounted<CefTranslatorTestRefPtrClientCToCpp,
    CefTranslatorTestRefPtrClient,
    cef_translator_test_ref_ptr_client_t>::UnwrapDerived(CefWrapperType type,
    CefTranslatorTestRefPtrClient* c) {
  if (type == WT_TRANSLATOR_TEST_REF_PTR_CLIENT_CHILD) {
    return reinterpret_cast<cef_translator_test_ref_ptr_client_t*>(
        CefTranslatorTestRefPtrClientChildCToCpp::Unwrap(
        reinterpret_cast<CefTranslatorTestRefPtrClientChild*>(c)));
  }
  NOTREACHED() << "Unexpected class type: " << type;
  return NULL;
}

#if DCHECK_IS_ON()
template<> base::AtomicRefCount CefCToCppRefCounted<CefTranslatorTestRefPtrClientCToCpp,
    CefTranslatorTestRefPtrClient,
    cef_translator_test_ref_ptr_client_t>::DebugObjCt = 0;
#endif

template<> CefWrapperType CefCToCppRefCounted<CefTranslatorTestRefPtrClientCToCpp,
    CefTranslatorTestRefPtrClient,
    cef_translator_test_ref_ptr_client_t>::kWrapperType =
    WT_TRANSLATOR_TEST_REF_PTR_CLIENT;
