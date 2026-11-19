// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <optional>

#include "render_process_handler.h"

#include <include/cef_parser.h>
#include <map>
#include <string>
#include "include/base/cef_logging.h"
#include "util.hpp"

const char kMessageName[] = "RenderProcessHandler";

RenderProcessHandler::RenderProcessHandler() {}

void RenderProcessHandler::SendLogMessage(CefRefPtr<CefFrame> frame,
                                          const cef_log_severity_t severity,
                                          const CefString& message) {
  if (!frame)
    return;

  if (severity == LOGSEVERITY_DEFAULT || severity == LOGSEVERITY_VERBOSE ||
      severity == LOGSEVERITY_INFO) {
    LOG(INFO) << message;
  } else if (severity == LOGSEVERITY_WARNING) {
    LOG(WARNING) << message;
  } else if (severity == LOGSEVERITY_ERROR) {
    LOG(ERROR) << message;
  } else if (severity == LOGSEVERITY_FATAL) {
    LOG(FATAL) << message;
  }

  // Package as an OnLogMessage notification to the host
  auto request = CefDictionaryValue::Create();
  request->SetString("id", uuid_to_string(CreateUuid()));
  request->SetString("class", "Browser");
  request->SetString("method", "OnLogMessage");
  request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());

  auto arguments = CefDictionaryValue::Create();
  arguments->SetInt("severity", severity);
  arguments->SetString("message", message);
  arguments->SetString("process", "Renderer");
  request->SetDictionary("arguments", arguments);

  auto json = CefValue::Create();
  json->SetDictionary(request);

  auto processMessage =
      CefProcessMessage::Create(kMessageName);
  processMessage->GetArgumentList()->SetValue(0, json);
  frame->SendProcessMessage(PID_BROWSER, processMessage);
}

void RenderProcessHandler::SendTraverseRequest(
    CefRefPtr<CefFrame> frame,
    const TraverseRequest& event) {
  CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
  request->SetString("id", uuid_to_string(CreateUuid()));
  request->SetString("class", "Browser");
  request->SetString("method", "OnTraverseRequest");
  request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());

  CefRefPtr<CefDictionaryValue> arguments = CefDictionaryValue::Create();
  CefRefPtr<CefDictionaryValue> sourceDict = CefDictionaryValue::Create();
  sourceDict->SetString("frameId", event.source.frameId);
  sourceDict->SetBool("isMainFrame", event.source.isMainFrame);
  arguments->SetDictionary("source", sourceDict);
  arguments->SetInt("delta", event.delta);
  arguments->SetString("key", event.key);
  if (event.info) {
    arguments->SetValue("info", event.info);
  } else {
    arguments->SetNull("info");
  }
  request->SetDictionary("arguments", arguments);

  CefRefPtr<CefValue> json = CefValue::Create();
  json->SetDictionary(request);
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create(kMessageName);
  message->GetArgumentList()->SetValue(0, json);
  frame->SendProcessMessage(PID_BROWSER, message);
}

void RenderProcessHandler::SendStateUpdate(
    CefRefPtr<CefFrame> frame,
    const CefString& navigationType,
    const StateUpdate& event) {
  CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
  request->SetString("id", uuid_to_string(CreateUuid()));
  request->SetString("class", "Browser");
  request->SetString("method",
                     navigationType == "push" ? "OnPushState" : "OnReplaceState");
  request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());

  CefRefPtr<CefDictionaryValue> arguments = CefDictionaryValue::Create();
  CefRefPtr<CefDictionaryValue> sourceDict = CefDictionaryValue::Create();
  sourceDict->SetString("frameId", event.source.frameId);
  sourceDict->SetBool("isMainFrame", event.source.isMainFrame);
  arguments->SetDictionary("source", sourceDict);

  arguments->SetString("url", event.url);
  arguments->SetValue("state", event.state);

  request->SetDictionary("arguments", arguments);

  CefRefPtr<CefValue> json = CefValue::Create();
  json->SetDictionary(request);
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create(kMessageName);
  message->GetArgumentList()->SetValue(0, json);
  frame->SendProcessMessage(PID_BROWSER, message);
}

CefRefPtr<CefRenderProcessHandler>
RenderProcessHandler::GetRenderProcessHandler() {
  return this;
}

void RenderProcessHandler::OnWebKitInitialized() {}

void RenderProcessHandler::OnBrowserCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDictionaryValue> extra_info) {}

void RenderProcessHandler::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {}

class MouseOverHandler : public CefV8Handler {
 public:
  MouseOverHandler() {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();
    CefRefPtr<CefV8Value> event = arguments.front();
    CefV8ValueList stopPropagationArguments;
    event->GetValue("stopPropagation")
        ->ExecuteFunction(event, stopPropagationArguments);
    CefRefPtr<CefV8Value> target = event->GetValue("target");
    CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
    request->SetString("id", uuid_to_string(CreateUuid()));
    request->SetString("class", "Browser");
    request->SetString("method", "OnMouseOver");
    request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());
    CefRefPtr<CefDictionaryValue> requestArguments =
        CefDictionaryValue::Create();
    std::string tagName = target->GetValue("tagName")->GetStringValue();
    requestArguments->SetString("tagName", tagName);
    if (tagName == "INPUT") {
      requestArguments->SetString("inputType",
                                  target->GetValue("type")->GetStringValue());
    }
    CefV8ValueList closestArguments;
    closestArguments.push_back(CefV8Value::CreateString("a"));
    CefRefPtr<CefV8Value> closestResult =
        target->GetValue("closest")->ExecuteFunction(target, closestArguments);
    CefV8ValueList getBoundingClientRectArguments;
    CefRefPtr<CefV8Value> v8Rectangle;
    if (closestResult->IsObject()) {
      std::string href = closestResult->GetValue("href")->GetStringValue();
      if (!href.empty()) {
        request->SetString("href", href);
      }
      v8Rectangle =
          closestResult->GetValue("getBoundingClientRect")
              ->ExecuteFunction(closestResult, getBoundingClientRectArguments);
    } else {
      v8Rectangle =
          target->GetValue("getBoundingClientRect")
              ->ExecuteFunction(target, getBoundingClientRectArguments);
    }
    if (v8Rectangle->IsObject()) {
      CefRefPtr<CefDictionaryValue> rectangle = CefDictionaryValue::Create();
      rectangle->SetInt("x", v8Rectangle->GetValue("x")->GetIntValue());
      rectangle->SetInt("y", v8Rectangle->GetValue("y")->GetIntValue());
      rectangle->SetInt("width", v8Rectangle->GetValue("width")->GetIntValue());
      rectangle->SetInt("height",
                        v8Rectangle->GetValue("height")->GetIntValue());
      requestArguments->SetDictionary("rectangle", rectangle);
    }
    request->SetDictionary("arguments", requestArguments);
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(request);
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kMessageName);
    message->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(PID_BROWSER, message);
    return true;
  }

  // Provide the reference counting implementation for this class.
  IMPLEMENT_REFCOUNTING(MouseOverHandler);
};

class HistoryTraversalOverrides : public CefV8Handler {
 public:
  explicit HistoryTraversalOverrides(CefRefPtr<RenderProcessHandler> renderProcessHandler)
      : renderProcessHandler(renderProcessHandler) {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();

    int delta = 0;
    if (name == "back") {
      delta = -1;
    } else if (name == "forward") {
      delta = 1;
    } else if (name == "go") {
      if (!arguments.empty() && arguments[0]->IsInt()) {
        delta = arguments[0]->GetIntValue();
      }
    }

    TraverseRequest request;
    request.source.frameId = frame->GetIdentifier();
    request.source.isMainFrame = frame->IsMain();
    request.delta = delta;
    request.key = "";
    request.info = nullptr;
    renderProcessHandler->SendTraverseRequest(frame, request);
    retval = CefV8Value::CreateUndefined();
    return true;
  }

 private:
  CefRefPtr<RenderProcessHandler> renderProcessHandler;

  IMPLEMENT_REFCOUNTING(HistoryTraversalOverrides);
};

class NavigationTraversalOverrides : public CefV8Handler {
 public:
  explicit NavigationTraversalOverrides(
      CefRefPtr<RenderProcessHandler> renderProcessHandler)
      : renderProcessHandler(renderProcessHandler) {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();

    int delta = 0;
    CefString key = "";
    CefRefPtr<CefV8Value> infoVal = nullptr;

    if (name == "back") {
      delta = -1;
      if (!arguments.empty() && arguments[0]->IsObject()) {
        infoVal = arguments[0]->GetValue("info");
      }
    } else if (name == "forward") {
      delta = 1;
      if (!arguments.empty() && arguments[0]->IsObject()) {
        infoVal = arguments[0]->GetValue("info");
      }
    } else if (name == "traverseTo") {
      if (!arguments.empty() && arguments[0]->IsString()) {
        key = arguments[0]->GetStringValue();
      }
      if (arguments.size() > 1 && arguments[1]->IsObject()) {
        infoVal = arguments[1]->GetValue("info");
      }
    }

    TraverseRequest request;
    request.source.frameId = frame->GetIdentifier();
    request.source.isMainFrame = frame->IsMain();
    request.delta = delta;
    request.key = key;
    if (infoVal) {
      request.info = CefV8ValueToCefValue(infoVal);
    }
    renderProcessHandler->SendTraverseRequest(frame, request);

    // Return NavigationResult { committed, finished }
    CefRefPtr<CefV8Value> global = context->GetGlobal();
    CefRefPtr<CefV8Value> navObj = global->GetValue("navigation");
    CefRefPtr<CefV8Value> currentEntry = (navObj && navObj->IsObject())
                                             ? navObj->GetValue("currentEntry")
                                             : nullptr;

    CefRefPtr<CefV8Value> promiseClass = global->GetValue("Promise");
    CefRefPtr<CefV8Value> resolveFunc = promiseClass->GetValue("resolve");
    CefV8ValueList resolveArgs;
    if (currentEntry) {
        resolveArgs.push_back(currentEntry);
    }
    CefRefPtr<CefV8Value> resolvedPromise =
        resolveFunc->ExecuteFunction(promiseClass, resolveArgs);

    retval = CefV8Value::CreateObject(nullptr, nullptr);
    retval->SetValue("committed", resolvedPromise,
                        V8_PROPERTY_ATTRIBUTE_NONE);
    retval->SetValue("finished", resolvedPromise,
                        V8_PROPERTY_ATTRIBUTE_NONE);
    return true;
   }

 private:
  CefRefPtr<RenderProcessHandler> renderProcessHandler;

  IMPLEMENT_REFCOUNTING(NavigationTraversalOverrides);
};

class CurrentEntryChangeHandler : public CefV8Handler {
 public:
  explicit CurrentEntryChangeHandler(
      CefRefPtr<RenderProcessHandler> renderProcessHandler)
      : renderProcessHandler(renderProcessHandler) {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    if (arguments.empty() || !arguments[0]->IsObject()) {
      return false;
    }

    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();
    CefRefPtr<CefV8Value> event = arguments[0];

    std::string navigationType = "push";
    CefRefPtr<CefV8Value> navigationTypeValue =
        event->GetValue("navigationType");
    if (navigationTypeValue && navigationTypeValue->IsString()) {
      navigationType = navigationTypeValue->GetStringValue().ToString();
    }

    if (navigationType != "push" && navigationType != "replace") {
      renderProcessHandler->SendLogMessage(
          frame, LOGSEVERITY_WARNING,
          "CurrentEntryChangeHandler ignored unexpected navigationType: " +
              navigationType);
      return true;
    }

    CefRefPtr<CefV8Value> globalObject = context->GetGlobal();
    CefRefPtr<CefV8Value> navigationObject =
        globalObject->GetValue("navigation");
    if (!navigationObject || !navigationObject->IsObject()) {
      return true;
    }

    CefRefPtr<CefV8Value> currentEntry =
        navigationObject->GetValue("currentEntry");
    if (!currentEntry || !currentEntry->IsObject()) {
      return true;
    }

    CefString url = currentEntry->GetValue("url")->GetStringValue();

    CefRefPtr<CefV8Value> stateValue = nullptr;
    CefRefPtr<CefV8Value> getStateFunction = currentEntry->GetValue("getState");
    if (getStateFunction && getStateFunction->IsFunction()) {
      CefV8ValueList emptyArguments;
      stateValue =
          getStateFunction->ExecuteFunction(currentEntry, emptyArguments);
    }

    StateUpdate stateUpdate;
    stateUpdate.source.frameId = frame->GetIdentifier();
    stateUpdate.source.isMainFrame = frame->IsMain();
    stateUpdate.url = url.ToString();
    stateUpdate.state = CefV8ValueToCefValue(stateValue);

    renderProcessHandler->SendStateUpdate(frame, navigationType, stateUpdate);
    return true;
  }

 private:
  CefRefPtr<RenderProcessHandler> renderProcessHandler;

  IMPLEMENT_REFCOUNTING(CurrentEntryChangeHandler);
};

class HashChangeHandler : public CefV8Handler {
 public:
  explicit HashChangeHandler(
      CefRefPtr<RenderProcessHandler> renderProcessHandler)
      : renderProcessHandler(renderProcessHandler) {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    if (arguments.empty() || !arguments[0]->IsObject()) {
      return false;
    }

    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();
    CefRefPtr<CefV8Value> event = arguments[0];

    CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
    request->SetString("id", uuid_to_string(CreateUuid()));
    request->SetString("class", "Browser");
    request->SetString("method", "OnHashChange");
    request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());

    CefRefPtr<CefDictionaryValue> requestArguments =
        CefDictionaryValue::Create();
    requestArguments->SetString("frameId", frame->GetIdentifier());
    requestArguments->SetBool("isMainFrame", frame->IsMain());
    requestArguments->SetString("oldUrl",
                                event->GetValue("oldURL")->GetStringValue());
    requestArguments->SetString("newUrl",
                                event->GetValue("newURL")->GetStringValue());

    request->SetDictionary("arguments", requestArguments);

    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(request);
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kMessageName);
    message->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(PID_BROWSER, message);

    return true;
  }

 private:
  CefRefPtr<RenderProcessHandler> renderProcessHandler;

  IMPLEMENT_REFCOUNTING(HashChangeHandler);
};

class FocusOutHandler : public CefV8Handler {
 public:
  FocusOutHandler() {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) override {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    CefRefPtr<CefFrame> frame = context->GetFrame();
    CefRefPtr<CefV8Value> event = arguments.front();
    CefRefPtr<CefV8Value> relatedTarget = event->GetValue("relatedTarget");
    CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
    request->SetString("id", uuid_to_string(CreateUuid()));
    request->SetString("class", "Browser");
    request->SetString("method", "OnMouseOver");
    request->SetInt("instanceId", frame->GetBrowser()->GetIdentifier());
    CefRefPtr<CefDictionaryValue> requestArguments =
        CefDictionaryValue::Create();
    if (!relatedTarget->IsNull()) {
      requestArguments->SetString(
          "tagName", relatedTarget->GetValue("tagName")->GetStringValue());
      CefRefPtr<CefV8Value> attributes = relatedTarget->GetValue("attributes");
      std::string inputType = attributes->GetValue("type")->GetStringValue();
      if (!inputType.empty()) {
        requestArguments->SetString("inputType", inputType);
      }
      requestArguments->SetBool(
          "isEditable", attributes->GetValue("isEditable")->GetBoolValue());
    }
    request->SetDictionary("arguments", requestArguments);
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(request);
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kMessageName);
    message->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(PID_BROWSER, message);
    return true;
  }

  // Provide the reference counting implementation for this class.
  IMPLEMENT_REFCOUNTING(FocusOutHandler);
};

void RenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                            CefRefPtr<CefFrame> frame,
                                            CefRefPtr<CefV8Context> context) {
  this->SendLogMessage(frame, LOGSEVERITY_INFO, "Entered OnContextCreated");
  CefRefPtr<CefV8Value> window = context->GetGlobal();

  // mouse over
  CefRefPtr<CefV8Handler> mouseOverHandler = new MouseOverHandler();
  CefV8ValueList mouseOverArguments;
  mouseOverArguments.push_back(CefV8Value::CreateString("mouseover"));
  mouseOverArguments.push_back(
      CefV8Value::CreateFunction("onMouseOver", mouseOverHandler));
  window->GetValue("addEventListener")
      ->ExecuteFunction(window, mouseOverArguments);

  // navigation
  CefRefPtr<CefV8Value> navigation = window->GetValue("navigation");
  if (navigation && navigation->IsObject()) {
    // current entry change
    CefRefPtr<CefV8Handler> entryChangeHandler =
        new CurrentEntryChangeHandler(this);
    CefV8ValueList entryChangeArgs;
    entryChangeArgs.push_back(CefV8Value::CreateString("currententrychange"));
    entryChangeArgs.push_back(
        CefV8Value::CreateFunction("onCurrentEntryChange", entryChangeHandler));
    navigation->GetValue("addEventListener")
        ->ExecuteFunction(navigation, entryChangeArgs);

    // navigation traversals
    CefRefPtr<CefV8Handler> navigationOverridesHandler =
        new NavigationTraversalOverrides(this);
    navigation->SetValue(
        "back", CefV8Value::CreateFunction("back", navigationOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    navigation->SetValue(
        "forward",
        CefV8Value::CreateFunction("forward", navigationOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    navigation->SetValue(
        "traverseTo",
        CefV8Value::CreateFunction("traverseTo", navigationOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
  }

  // history traversals
  CefRefPtr<CefV8Value> history = window->GetValue("history");
  if (history && history->IsObject()) {
    CefRefPtr<CefV8Handler> historyOverridesHandler =
        new HistoryTraversalOverrides(this);
    history->SetValue(
        "back", CefV8Value::CreateFunction("back", historyOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    history->SetValue(
        "forward",
        CefV8Value::CreateFunction("forward", historyOverridesHandler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    history->SetValue("go",
                      CefV8Value::CreateFunction("go", historyOverridesHandler),
                      V8_PROPERTY_ATTRIBUTE_NONE);
  }

  // hash change
  CefRefPtr<CefV8Handler> hashChangeHandler = new HashChangeHandler(this);
  CefV8ValueList hashArgs;
  hashArgs.push_back(CefV8Value::CreateString("hashchange"));
  hashArgs.push_back(
      CefV8Value::CreateFunction("onHashChange", hashChangeHandler));
  window->GetValue("addEventListener")->ExecuteFunction(window, hashArgs);

  // focus out
  CefRefPtr<CefV8Value> document = window->GetValue("document");
  CefRefPtr<CefV8Handler> focusOutHandler = new FocusOutHandler();
  CefV8ValueList focusOutArguments;
  focusOutArguments.push_back(CefV8Value::CreateString("focusout"));
  focusOutArguments.push_back(
      CefV8Value::CreateFunction("onFocusOut", focusOutHandler));
  document->GetValue("addEventListener")
      ->ExecuteFunction(document, focusOutArguments);
}

void RenderProcessHandler::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                             CefRefPtr<CefFrame> frame,
                                             CefRefPtr<CefV8Context> context) {}

void RenderProcessHandler::OnUncaughtException(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context,
    CefRefPtr<CefV8Exception> exception,
    CefRefPtr<CefV8StackTrace> stackTrace) {}

void RenderProcessHandler::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                                CefRefPtr<CefFrame> frame,
                                                CefRefPtr<CefDOMNode> node) {
  if (node.get()) {
    CefRefPtr<CefDictionaryValue> request = CefDictionaryValue::Create();
    request->SetString("id", uuid_to_string(CreateUuid()));
    request->SetString("class", "Browser");
    request->SetString("method", "OnFocusedNodeChanged");
    request->SetInt("instanceId", browser->GetIdentifier());
    CefRefPtr<CefDictionaryValue> arguments = CefDictionaryValue::Create();
    arguments->SetString("tagName", node->GetElementTagName());
    std::string inputType = node->GetElementAttribute("type");
    if (!inputType.empty()) {
      arguments->SetString("inputType", inputType);
    }
    arguments->SetBool("isEditable", node->IsEditable());
    request->SetDictionary("arguments", arguments);
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(request);
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(kMessageName);
    message->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(PID_BROWSER, message);
  }
}

// Custom handler for our promise "then" callback
class PromiseThenHandler : public CefV8Handler {
 public:
  explicit PromiseThenHandler(CefRefPtr<CefFrame> frame,
                              CefProcessId sourceProcessId,
                              const CefString& requestId)
      : frame(frame), sourceProcessId(sourceProcessId), requestId(requestId) {}

  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) override {
    if (name != "onPromiseResolved" || arguments.size() == 0) {
      return false;
    }

    CefRefPtr<CefProcessMessage> responseMessage =
        CefProcessMessage::Create(kMessageName);
    CefRefPtr<CefDictionaryValue> messageArguments =
        CefDictionaryValue::Create();
    CefRefPtr<CefDictionaryValue> response = CefDictionaryValue::Create();
    response->SetString("id", requestId);
    response->SetBool("success", true);
    response->SetValue("result", CefV8ValueToCefValue(arguments[0]));
    CefRefPtr<CefValue> json = CefValue::Create();
    json->SetDictionary(response);
    responseMessage->GetArgumentList()->SetValue(0, json);
    frame->SendProcessMessage(sourceProcessId, responseMessage);
    retval = arguments[0];
    return true;
  }

 private:
  CefRefPtr<CefFrame> frame;
  CefProcessId sourceProcessId;
  CefString requestId;
  IMPLEMENT_REFCOUNTING(PromiseThenHandler);
};

bool RenderProcessHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  DCHECK_EQ(source_process, PID_BROWSER);
  CefRefPtr<CefV8Context> context = frame->GetV8Context();
  context->Enter();
  const CefString& name = message->GetName();
  bool handled = false;
  if (name == "Eval") {
    auto argumentList = message->GetArgumentList();
    const CefString& requestId = argumentList->GetString(0);
    const CefString& code = argumentList->GetString(1);
    const CefString& scriptUrl = argumentList->GetString(2);
    const int startLine = argumentList->GetInt(3);
    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;
    bool success = context->Eval(code, scriptUrl, startLine, retval, exception);
    if (success && retval->IsPromise()) {
      CefRefPtr<CefV8Value> thenFunction = retval->GetValue("then");
      CefRefPtr<PromiseThenHandler> handler =
          new PromiseThenHandler(frame, source_process, requestId);
      CefRefPtr<CefV8Value> onResolvedFunc =
          CefV8Value::CreateFunction("onPromiseResolved", handler);
      thenFunction->ExecuteFunction(retval, {onResolvedFunc});
    } else {
      CefRefPtr<CefDictionaryValue> response = CefDictionaryValue::Create();
      response->SetString("id", requestId);
      response->SetBool("success", success);
      if (success) {
        response->SetValue("result", CefV8ValueToCefValue(retval));
      } else {
        CefRefPtr<CefDictionaryValue> error = CefDictionaryValue::Create();
        error->SetInt("endColumn", exception->GetEndColumn());
        error->SetInt("endPosition", exception->GetEndPosition());
        error->SetInt("lineNumber", exception->GetLineNumber());
        error->SetString("message", exception->GetMessage());
        error->SetString("scriptResourceName",
                         exception->GetScriptResourceName());
        error->SetString("sourceLine", exception->GetSourceLine());
        error->SetInt("startColumn", exception->GetStartColumn());
        error->SetInt("startPosition", exception->GetStartPosition());
        response->SetDictionary("error", error);
      }
      CefRefPtr<CefProcessMessage> responseMessage =
          CefProcessMessage::Create(kMessageName);
      CefRefPtr<CefValue> json = CefValue::Create();
      json->SetDictionary(response);
      responseMessage->GetArgumentList()->SetValue(0, json);
      frame->SendProcessMessage(source_process, responseMessage);
    }
    handled = true;
  }
  context->Exit();
  return handled;
}