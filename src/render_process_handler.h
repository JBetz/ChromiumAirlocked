// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#pragma once

#include <optional>
#include <set>

#include "process_handler.h"

struct NavigationSource {
  std::string frameId;
  bool isMainFrame = false;
};

struct TraverseRequest {
  NavigationSource source;
  int delta = 0;    // -1, 1, or explicit delta
  std::string key;  // target key for traverseTo
  CefRefPtr<CefValue> info;
};

struct StateUpdate {
  NavigationSource source;
  std::string url;
  CefRefPtr<CefValue> state;
};

// Client app implementation for the renderer process.
class RenderProcessHandler : public ProcessHandler,
                             public CefRenderProcessHandler {
 public:
  RenderProcessHandler();

  void SendLogMessage(CefRefPtr<CefFrame> frame,
                      const cef_log_severity_t severity,
                      const CefString& message);
  void SendTraverseRequest(CefRefPtr<CefFrame> frame,
                          const TraverseRequest& request);
  void SendStateUpdate(CefRefPtr<CefFrame> frame,
                       const CefString& navigationType,
                       const StateUpdate& request);

 private:
  // CefApp methods.
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

  // CefRenderProcessHandler methods.
  void OnWebKitInitialized() override;
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override;
  void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override;
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;
  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override;
  void OnUncaughtException(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context,
                           CefRefPtr<CefV8Exception> exception,
                           CefRefPtr<CefV8StackTrace> stackTrace) override;
  void OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefDOMNode> node) override;
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

 private:
  bool last_node_is_editable_ = false;

  IMPLEMENT_REFCOUNTING(RenderProcessHandler);
  DISALLOW_COPY_AND_ASSIGN(RenderProcessHandler);
};