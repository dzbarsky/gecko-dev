/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/**
 * MODULE NOTES:
 * @update  gpk 06/18/98
 * 
 *         
 */

#include "nsXIFDTD.h" 
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsParserTypes.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsTokenHandler.h"
#include "nsIDTDDebug.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLContentSinkStream.h"
#include "prmem.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_XIF_DTD_IID); 

static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";
static const char* kXIFTextContentType = "text/xif";
static const char* kXIFDocHeader= "<!DOCTYPE xif>";

static nsAutoString gEmpty;
  

struct nsXIFTagEntry {
  char      mName[32];
  eXIFTags  fTagID;
};

  // KEEP THIS LIST SORTED!
  // NOTE: This table is sorted in ascii collating order. If you
  // add a new entry, make sure you put it in the right spot otherwise
  // the binary search code above will break! 
nsXIFTagEntry gXIFTagTable[] =
{
  {"!!UNKNOWN",             eXIFTag_unknown},
  {"!DOCTYPE",              eXIFTag_doctype},
  {"?XML",                  eXIFTag_xml},

  {"attr",                  eXIFTag_attr},
  {"comment",               eXIFTag_comment},
  {"container",             eXIFTag_container},
  {"content",               eXIFTag_content},

  {"css_declaration",       eXIFTag_css_declaration},
  {"css_declaration_list",  eXIFTag_css_declaration_list},
  {"css_rule",              eXIFTag_css_rule},
  {"css_selector",          eXIFTag_css_selector},
  {"css_stylelist",         eXIFTag_css_stylelist},
  {"css_stylerule",         eXIFTag_css_stylerule},
  {"css_stylesheet",        eXIFTag_css_stylesheet},

  {"import",                eXIFTag_import},

  {"leaf",                  eXIFTag_leaf},
  {"link",                  eXIFTag_link},

  {"section",               eXIFTag_section},
  {"section_body",          eXIFTag_section_body}, 
  {"section_head",          eXIFTag_section_head}, 
  {"stylelist",             eXIFTag_stylelist},

  {"url",                   eXIFTag_url},
};

struct nsXIFAttrEntry
{
  char            mName[11];
  eXIFAttributes  fAttrID;
};

nsXIFAttrEntry gXIFAttributeTable[] =
{
  {"key",     eXIFAttr_key},
  {"tag",     eXIFAttr_tag},
  {"value",   eXIFAttr_value},  
};



/*
 *  This method iterates the tagtable to ensure that is 
 *  is proper sort order. This method only needs to be
 *  called once.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
class nsXIFTagTableVerifier {
public:
  nsXIFTagTableVerifier()
  {
    PRInt32  count=sizeof(gXIFTagTable)/sizeof(nsXIFTagEntry);
    PRInt32 i,j;
    for(i=1;i<count-1;i++)
    {
      j=strcmp(gXIFTagTable[i-1].mName,gXIFTagTable[i].mName);
      if(j>0) {
        cout << "XIF Tag Element Table is out of order at " << i << "!" << endl;
        return;
      }
    }

    count = sizeof(gXIFAttributeTable)/sizeof(nsXIFAttrEntry);
    for(i=1;i<count-1;i++)
    {
      j=strcmp(gXIFAttributeTable[i-1].mName,gXIFAttributeTable[i].mName);
      if(j>0) {
        cout << "XIF Tag Attribute Table is out of order at " << i << "!" << endl;
        return;
      }
    }
    return;
  }
};


/*
 *  This method accepts a string (and optionally, its length)
 *  and determines the eXIFTag (id) value.
 *  
 *  @update  gess 3/25/98
 *  @param   aString -- string to be convered to id
 *  @return  valid id, or user_defined.
 */
eXIFTags DetermineXIFTagType(const nsString& aString)
{
  PRInt32  result=-1;
  PRInt32  cnt=sizeof(gXIFTagTable)/sizeof(nsXIFTagEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=kNotFound;
  
  while(low<=high){
    middle=(PRInt32)(low+high)/2;
    result=aString.Compare(gXIFTagTable[middle].mName, PR_TRUE);
    if (result==0)
      return gXIFTagTable[middle].fTagID; 
    if (result<0)
      high=middle-1; 
    else low=middle+1; 
  }
  return eXIFTag_userdefined;
}




/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gpk 06/18/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsXIFDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsXIFDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsHTMLParser.
 *  
 *  @update  gpk 06/18/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewXIFDTD(nsIDTD** aInstancePtrResult)
{
  nsXIFDTD* it = new nsXIFDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsXIFDTD)
NS_IMPL_RELEASE(nsXIFDTD)



/**
 *  
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
PRInt32 XIFDispatchTokenHandler(CToken* aToken,nsIDTD* aDTD) {
  
  PRInt32         result=0;

  eHTMLTokenTypes theType= (eHTMLTokenTypes)aToken->GetTokenType();  
  nsXIFDTD*       theDTD=(nsXIFDTD*)aDTD;

  nsString& name = aToken->GetStringValue();
  eXIFTags type = DetermineXIFTagType(name);
  
  if (type != eXIFTag_userdefined)
    aToken->SetTypeID(type);
  
  if(aDTD) {
    switch(theType) {
      case eToken_start:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_end:
        result=theDTD->HandleEndToken(aToken); break;
      case eToken_comment:
        result=theDTD->HandleCommentToken(aToken); break;
      case eToken_entity:
        result=theDTD->HandleEntityToken(aToken); break;
      case eToken_whitespace:
        result=theDTD->HandleWhiteSpaceToken(aToken); break;
      case eToken_newline:
        result=theDTD->HandleWhiteSpaceToken(aToken); break;
      case eToken_text:
        result=theDTD->HandleTextToken(aToken); break;
      case eToken_attribute:
        result=theDTD->HandleAttributeToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}

/**
 *  init the set of default token handlers...
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
void nsXIFDTD::InitializeDefaultTokenHandlers() {
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_start));

  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_end));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_comment));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_entity));

  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_whitespace));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_newline));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_text));
  
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_attribute));
//  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_script));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_style));
  AddTokenHandler(new CTokenHandler(XIFDispatchTokenHandler,eToken_skippedcontent));
}

class nsXIfTokenDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
    return 0;
  }
};


static nsXIfTokenDeallocator gTokenKiller;

/**
 *  Default constructor
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
nsXIFDTD::nsXIFDTD() : nsIDTD(), mTokenDeque(gTokenKiller)  {
  mParser=0;
  nsCRT::zero(mContextStack,sizeof(mContextStack));
  nsCRT::zero(mTokenHandlers,sizeof(mTokenHandlers));
  
  mHTMLStackPos = 0;
  memset(mHTMLTagStack,0,sizeof(mHTMLTagStack));
  memset(mHTMLNameStack,0,sizeof(mHTMLNameStack));

  
  mContextStackPos=0;
  mHasOpenForm=PR_FALSE;
  mHasOpenMap=PR_FALSE;
  InitializeDefaultTokenHandlers();
  mContextStackPos=0;
  mContextStack[mContextStackPos++]=eXIFTag_unknown;
  mDTDDebug=nsnull;
  mInContent=PR_FALSE;
  mLowerCaseAttributes=PR_TRUE;
  mLowerCaseTags=PR_TRUE;
}

/**
 *  Default destructor
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
nsXIFDTD::~nsXIFDTD(){
  DeleteTokenHandlers();
//  NS_RELEASE(mSink);
}




/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update	gpk 07/09/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
PRBool nsXIFDTD::CanParse(nsString& aContentType, PRInt32 aVersion){
  PRBool result=aContentType.Equals(kXIFTextContentType);
  return result;
}


/**
 * 
 * @update	gpk 07/09/98
 * @param 
 * @return
 */
eAutoDetectResult nsXIFDTD::AutoDetectContentType(nsString& aBuffer,nsString& aType){
  eAutoDetectResult result=eUnknownDetect;
  if(kNotFound!=aBuffer.Find(kXIFDocHeader))
  {
    PRInt32 offset = aBuffer.Find("<section>");
    if (offset != -1)
      aBuffer.Cut(0,offset);
    aType = kXIFTextContentType;
    result=eValidDetect;
  }
  return result;
}


/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
PRInt32 nsXIFDTD::WillBuildModel(nsString& aFileName){
  PRInt32 result=0;

  if(mSink)
  {
#if defined(WIN32)
  const char* filename="c:\\temp\\save.html";
  mOut = new fstream(filename,ios::out);

  ((nsHTMLContentSinkStream*)mSink)->SetOutputStream(*mOut);

#endif

    mSink->WillBuildModel();
  }

  return result;
}

/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
PRInt32 nsXIFDTD::DidBuildModel(PRInt32 anErrorCode){
  PRInt32 result=0;

  if(mSink) 
  {
    mSink->DidBuildModel(anErrorCode);
    
    if (mOut != nsnull)
    {
      mOut->close();
      delete mOut;
    }

  }

  return result;
}


/**
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gpk 06/18/98
 *  @param   aType
 *  @param   aToken
 *  @param   aParser
 *  @return  
 */
PRInt32 nsXIFDTD::HandleToken(CToken* aToken){
  PRInt32 result=0;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    CTokenHandler*  aHandler=GetTokenHandler(theType);

    if(aHandler) {
      result=(*aHandler)(theToken,this);
    }

  }//if
  return result;
}



/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsXIFDTD::HandleWhiteSpaceToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*  st    = (CStartToken*)(aToken);
  eXIFTags      type  =(eXIFTags)st->GetTypeID();

  //Begin by gathering up attributes...
  nsCParserNode node((CHTMLToken*)aToken);
  PRInt16       attrCount=aToken->GetAttributeCount();
  PRInt32       result=(0==attrCount) ? kNoError : mParser->CollectAttributes(node,attrCount);

  if(kNoError==result)
  {
    if (mInContent == PR_TRUE)
      mSink->AddLeaf(node);
  } 
  return result;
}


/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsXIFDTD::HandleTextToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*    st    = (CStartToken*)(aToken);
  eXIFTags        type  =(eXIFTags)st->GetTypeID();
  nsCParserNode   node((CHTMLToken*)aToken);

  PRInt32 result = kNoError;

  if (type == eXIFTag_text)
  {
    nsString& temp = aToken->GetText();

    if (temp != "<xml version=\"1.0\"?>")
    {
      result= AddLeaf(node);
    }
  }

  return result;
}


void nsXIFDTD::AddAttribute(nsIParserNode& aNode)
{
  nsString key;
  nsString value;
  PRBool   hasValue;

  hasValue = GetAttributePair(aNode,key,value);
  CAttributeToken* attribute = new CAttributeToken(key,value);
  nsIParserNode* top = PeekNode();
  ((nsCParserNode*)top)->AddAttribute(attribute);
}


/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsXIFDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*  st    = (CStartToken*)(aToken);
  eXIFTags      type  =(eXIFTags)st->GetTypeID();

  //Begin by gathering up attributes...
  nsCParserNode   node = ((CHTMLToken*)aToken);
  PRInt16         attrCount=aToken->GetAttributeCount();
  PRInt32         result=(0==attrCount) ? kNoError : mParser->CollectAttributes(node,attrCount);

  if(kNoError==result)
  {
    switch (type)
    {
      case eXIFTag_container:
      case eXIFTag_leaf: 
        StartTopOfStack();
        result = OpenContainer(node);        
      break;

      case eXIFTag_content:
        StartTopOfStack();
        mInContent = PR_TRUE;
      break;

      case eXIFTag_attr:
        AddAttribute(node);
      break;

      case eXIFTag_css_stylesheet:
        StartTopOfStack();
        BeginCSSStyleSheet(node);
      break;

      case eXIFTag_css_rule:
        BeginCSSStyleRule(node);
      break;

      case eXIFTag_css_selector:
        AddCSSSelector(node);
      break;

      case eXIFTag_css_declaration_list:
        BeginCSSDeclarationList(node);
      break;

      case eXIFTag_css_declaration:
        AddCSSDeclaration(node);
      break;

    }
  } 
  return result;
}


/**
 *  This method gets called when an end token has been 
 *  encountered in the parse process. If the end tag matches
 *  the start tag on the stack, then simply close it. Otherwise,
 *  we have a erroneous state condition. This can be because we
 *  have a close tag with no prior open tag (user error) or because
 *  we screwed something up in the parse process. I'm not sure
 *  yet how to tell the difference.
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsXIFDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRInt32         result        = kNoError;
  CEndToken*      et            = (CEndToken*)(aToken);
  eXIFTags        tokenTagType  = (eXIFTags)et->GetTypeID();
  nsCParserNode   node          = ((CHTMLToken*)aToken);

  switch (tokenTagType)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
      StartTopOfStack();
      result=CloseContainer(node); 
    break;

    case eXIFTag_content:
      mInContent = PR_FALSE;
    break;
    
    case eXIFTag_css_stylesheet:
      mInContent = PR_FALSE;
      EndCSSStyleSheet(node);
    break;

    case  eXIFTag_css_rule:
      mInContent = PR_FALSE;
      EndCSSStyleRule(node);
    break;


    case eXIFTag_css_declaration_list:
      mInContent = PR_FALSE;
      EndCSSDeclarationList(node);
    break;
  }
  return result;
}

/**
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsXIFDTD::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CEntityToken* et = (CEntityToken*)(aToken);
  PRInt32       result=kNoError;
  eXIFTags      tokenTagType=(eXIFTags)et->GetTypeID();

  if(PR_FALSE==CanOmit(GetTopNode(),tokenTagType)) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when a comment token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the comment
 *  in the same code that we use for text.
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsXIFDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  return kNoError;
}

/**
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. This is an error, since
 *  all attributes should have been accounted for in the prior
 *  start or end tokens
 *  
 *  @update  gpk 06/18/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsXIFDTD::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  CAttributeToken*  at = (CAttributeToken*)(aToken);
  PRInt32 result=kNoError;
  return result;
}



/**
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gpk 06/18/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
void nsXIFDTD::DeleteTokenHandlers(void) {
  int i=0;
  for(i=eToken_unknown;i<eToken_last;i++){
    delete mTokenHandlers[i];
    mTokenHandlers[i]=0;
  }
  return;
}


/**
 *  Finds a tag handler for the given tag type.
 *  
 *  @update  gpk 06/18/98
 *  @param   aTagType type of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
CTokenHandler* nsXIFDTD::GetTokenHandler(eHTMLTokenTypes aType) const {
  CTokenHandler* result=0;
  if((aType>0) && (aType<eToken_last)) {
    result=mTokenHandlers[aType];
  } 
  else {
  }
  return result;
}


/**
 *  Register a handler.
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
CTokenHandler* nsXIFDTD::AddTokenHandler(CTokenHandler* aHandler) {
  NS_ASSERTION(0!=aHandler,"Error: Null handler");
  
  if(aHandler)  {
    eHTMLTokenTypes type=(eHTMLTokenTypes)aHandler->GetTokenType();
    if(type<eToken_last) {
      CTokenHandler* old=mTokenHandlers[type];
      mTokenHandlers[type]=aHandler;
    }
    else {
      //add code here to handle dynamic tokens...
    }
  }
  return 0;
}

/**
 * 
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return 
 */
void nsXIFDTD::SetParser(nsIParser* aParser) {
  mParser=(nsParser*)aParser;
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gpk 06/18/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* nsXIFDTD::SetContentSink(nsIContentSink* aSink) {
  nsIContentSink* old=mSink;
  mSink=(nsIHTMLContentSink*)aSink;


  return old;
}


 

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gpk 06/18/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool nsXIFDTD::CanContain(eXIFTags aParent,eXIFTags aChild) const {

  PRBool result=PR_FALSE;
  
  // Revisit -- for now, anybody can contain anything
  result = PR_TRUE;  
  
  return result;
}


/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gpk 06/18/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool nsXIFDTD::CanContainIndirect(eXIFTags aParent,eXIFTags aChild) const {
  PRBool result=PR_FALSE;
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag can contain newlines. Most do not.
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::CanOmit(eXIFTags aParent,eXIFTags aChild) const {
  PRBool result=PR_FALSE;
  return result;
}


/**
 *  This method gets called to determine whether a given
 *  ENDtag can be omitted. Admittedly,this is a gross simplification.
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::CanOmitEndTag(eXIFTags aParent,eXIFTags aChild) const {
  PRBool result=PR_FALSE;
      
  switch(aChild) 
  {
    case eXIFTag_attr:
      result=PR_TRUE; 

    default:
      result=PR_FALSE;
  } 
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::IsXIFContainer(eXIFTags aTag) const {
  PRBool result=PR_FALSE;

  switch(aTag){
    case eXIFTag_attr:
    case eXIFTag_text:
    case eXIFTag_whitespace:
    case eXIFTag_newline:
      result=PR_FALSE;
    break;

    default:
      result=PR_TRUE;
  }
  return result;
}


/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gpk 06/18/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool nsXIFDTD::IsHTMLContainer(eHTMLTags aTag) const {
  PRBool result=PR_TRUE; // by default everything is a 

  switch(aTag) {
    case eHTMLTag_meta:
    case eHTMLTag_br:
    case eHTMLTag_hr:
      result=PR_FALSE;
    break;
  }
  return result;
}



/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
eXIFTags nsXIFDTD::GetDefaultParentTagFor(eXIFTags aTag) const{
  
  eXIFTags result=eXIFTag_unknown;
  
  switch(aTag) 
  {
    case eXIFTag_section:
      result=eXIFTag_unknown; break;

    case eXIFTag_section_body:
    case eXIFTag_section_head:
      result=eXIFTag_section; break;
   }
  return result;
}



/**
 * 
 * @update	gpk 06/18/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
PRInt32 nsXIFDTD::DidOpenContainer(eXIFTags aTag,PRBool /*anExplicitOpen*/){
  PRInt32   result=0;
  return result;
}

/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
PRInt32 nsXIFDTD::DidCloseContainer(eXIFTags aTag,PRBool/*anExplicitClosure*/){
  PRInt32 result=0;
  return result;
}


/**
 *  This method allows the caller to determine if a form
 *  element is currently open.
 *  
 *  @update  gpk 06/18/98
 *  @param   
 *  @return  
 */
PRBool nsXIFDTD::HasOpenContainer(eXIFTags aContainer) const {
  PRBool result=PR_FALSE;

  result=(kNotFound!=GetTopmostIndexOf(aContainer));

  return result;
}

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gpk 06/18/98
 *  @return  tag id of topmost node in contextstack
 */
eXIFTags nsXIFDTD::GetTopNode() const {
  if(mContextStackPos) 
    return mContextStack[mContextStackPos-1];
  return eXIFTag_unknown;
}


/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gpk 06/18/98
 *  @param   eXIFTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 nsXIFDTD::GetTopmostIndexOf(eXIFTags aTag) const {
  int i=0;
  for(i=mContextStackPos-1;i>=0;i--){
    if(mContextStack[i]==aTag)
      return i;
  }
  return kNotFound;
}



/**
  * Begin Support for converting from XIF to HTML
  *
  */









PRBool nsXIFDTD::GetAttribute(const nsIParserNode& aNode, const nsString& aKey, nsString& aValue)
{
  PRInt32   i;
  PRInt32   count = aNode.GetAttributeCount();
  for (i = 0; i < count; i++)
  {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.Equals(aKey))
    {
      const nsString& value = aNode.GetValueAt(i);
      aValue = value;
      aValue.StripChars("\"");
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


PRBool nsXIFDTD::GetAttributePair(nsIParserNode& aNode, nsString& aKey, nsString& aValue)
{
 
  PRInt32 count = aNode.GetAttributeCount();
  PRInt32 i;
  PRBool  hasValue = PR_FALSE;

  for (i = 0; i < count; i++)
  {
    const nsString& k = aNode.GetKeyAt(i);
    const nsString& v = aNode.GetValueAt(i);

    nsAutoString key(k);
    nsAutoString value(v);

    char*  quote = "\"";
 
    key.StripChars(quote);
    value.StripChars(quote);

    if (key.Equals("name"))
      aKey = value;
    if (key.Equals("value"))
    {
      aValue = value;
      hasValue = PR_TRUE;
    }
  }
  return hasValue;
}


eHTMLTags nsXIFDTD::GetHTMLTag(const nsString& aName)
{
  eHTMLTags  tag = eHTMLTag_unknown;
  
  if (aName.EqualsIgnoreCase("style"))
    tag = eHTMLTag_style;
  else
  {
    char buffer[256];

    aName.ToCString(buffer,255);
    tag = NS_TagToEnum(buffer);
  }
  return tag;
}



eHTMLTags nsXIFDTD::GetStartTag(const nsIParserNode& aNode, nsString& aName)
{
  eXIFTags  type = (eXIFTags)aNode.GetNodeType();  
  eHTMLTags tag = eHTMLTag_unknown;

  switch (type)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
      if (GetAttribute(aNode,nsString("isa"),aName))
        tag = GetHTMLTag(aName);
    break;

    case eXIFTag_css_stylesheet:
      aName = "style";
      tag = GetHTMLTag(aName);
    break;  
  }
  return tag;
}


void nsXIFDTD::PushHTMLTag(const eHTMLTags aTag, const nsString& aName)
{

  mHTMLTagStack[mHTMLStackPos]=aTag;
  mHTMLNameStack[mHTMLStackPos]=new nsAutoString(aName);
  mHTMLTagStack[++mHTMLStackPos]=eHTMLTag_unknown;
}

void nsXIFDTD::PopHTMLTag(eHTMLTags& aTag, nsString*& aName)
{
  NS_ASSERTION(mHTMLStackPos > 0,"The stack must not be empty");

  aTag = eHTMLTag_unknown;
  if (mHTMLStackPos > 0)
  {
    aTag = mHTMLTagStack[mHTMLStackPos-1];
    aName = mHTMLNameStack[mHTMLStackPos-1];
    mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
  }
}


void nsXIFDTD::PushNodeAndToken(nsString& aName)
{
  CToken*         token = new CStartToken(aName);
  nsCParserNode*  node = new nsCParserNode(token);

  mTokenStack.AppendElement((void*)token);
  mNodeStack.AppendElement((void*)node);  
}

nsIParserNode* nsXIFDTD::PeekNode()
{
  PRInt32 count = mNodeStack.Count()-1;
  
  if (count >= 0)
    return (nsIParserNode*)mNodeStack.ElementAt(mNodeStack.Count()-1);
  return nsnull;
}

CToken* nsXIFDTD::PeekToken()
{
  PRInt32 count = mTokenStack.Count()-1; 
  if (count >= 0)
    return (CToken*)mTokenStack.ElementAt(mTokenStack.Count()-1);
  return nsnull;
}

void nsXIFDTD::PopAndDelete()
{
  nsIParserNode* node = PeekNode();
  CToken* token = PeekToken();

  if (node != nsnull && token != nsnull)
  {
    mNodeStack.RemoveElement(node);
    mTokenStack.RemoveElement(token);
    
    delete node;
    delete token;
  }
}

PRBool nsXIFDTD::StartTopOfStack()
{
  // If something is already on the stack, then
  // emit it and pop it off the stack
  nsIParserNode* top = PeekNode();
  if (top != nsnull)
  {
    eHTMLTags  tag = (eHTMLTags)top->GetNodeType();
    
    if (IsHTMLContainer(tag))
    {
      mInContent = PR_TRUE;
      mSink->OpenContainer(*top);
    }
    else
      mSink->AddLeaf(*top);
    
    PopAndDelete();
    return PR_TRUE;
  }
  return PR_FALSE;
}


void nsXIFDTD::BeginStartTag(const nsIParserNode& aNode)
{
  eXIFTags  type = (eXIFTags)aNode.GetNodeType();
  eHTMLTags tag;
  nsString  tagName;

  switch (type)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
        tag = GetStartTag(aNode,tagName);
        if (type == eXIFTag_container)
          PushHTMLTag(tag,tagName);  
 
        CToken*         token = new CStartToken(tagName);
        nsCParserNode*  node = new nsCParserNode(token);
        PushNodeAndToken(tagName);
      break;
  }

}

void nsXIFDTD::AddEndTag(const nsIParserNode& aNode)
{
  // Get the top the HTML stack
  eHTMLTags   tag;
  nsString*   name = nsnull;
  PopHTMLTag(tag,name);
   
  // Create a parse node for form this token
  CEndToken     token(*name);
  nsCParserNode node(&token);

  // close the container 
  mSink->CloseContainer(node); 

  // delete the name
  if (name != nsnull)
    delete name;
 
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsXIFDTD::OpenContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  
  PRInt32   result=kNoError; 
  eXIFTags  type =(eXIFTags)aNode.GetNodeType();

  switch (type)
  {
    case eXIFTag_container:
    case eXIFTag_leaf:
      BeginStartTag(aNode);
    break;
  }    
  mContextStack[mContextStackPos++]=type;

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsXIFDTD::CloseContainer(const nsIParserNode& aNode)
{
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32   result=kNoError; //was false
  eXIFTags type=(eXIFTags)aNode.GetNodeType();
  
  if (type == eXIFTag_container)
    AddEndTag(aNode);

  mContextStack[--mContextStackPos]=eXIFTag_unknown;

 
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gpk 06/18/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsXIFDTD::AddLeaf(const nsIParserNode& aNode)
{
  eXIFTags  type = (eXIFTags)aNode.GetNodeType();    
  PRInt32 result=mSink->AddLeaf(aNode); 
  return result;
}

/**
 *  This method gets called to create a valid context stack
 *  for the given child. We compare the current stack to the
 *  default needs of the child, and push new guys onto the
 *  stack until the child can be properly placed.
 *
 *  @update  gpk 06/18/98
 *  @param   aChildTag is the child for whom we need to 
 *           create a new context vector
 *  @return  true if we succeeded, otherwise false
 */
PRInt32 nsXIFDTD::CreateContextStackFor(eXIFTags aChildTag)
{
  mContextStack[++mContextStackPos] = aChildTag;
  return kNoError;
}




/*******************************************************************
  These methods used to be hidden in the tokenizer-delegate. 
  That file merged with the DTD, since the separation wasn't really
  buying us anything.
 *******************************************************************/

/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of some kind of tagged 
 *  element. We don't know yet if it's a tag or a comment.
 *  
 *  @update  gpk 06/18/98
 *  @param   aChar is the last char read
 *  @param   aScanner is represents our input source
 *  @param   aToken is the out arg holding our new token
 *  @return  error code (may return kInterrupted).
 */
PRInt32 nsXIFDTD::ConsumeTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {

  nsAutoString empty("");
  PRInt32 result=aScanner.GetChar(aChar);

  if(kNoError==result) {

    switch(aChar) {
      case kForwardSlash:
        PRUnichar ch; 
        result=aScanner.Peek(ch);
        if(kNoError==result) {
          if(nsString::IsAlpha(ch))
            aToken=new CEndToken(empty);
          else aToken=new CCommentToken(empty); //Special case: </ ...> is treated as a comment
        }//if
        break;
      case kExclamation:
        aToken=new CCommentToken(empty);
        break;
      default:
        if(nsString::IsAlpha(aChar))
          return ConsumeStartTag(aChar,aScanner,aToken);
        else if(kEOF!=aChar) {
          nsAutoString temp("<");
          return ConsumeText(temp,aScanner,aToken);
        }
    } //switch

    if((0!=aToken) && (kNoError==result)) {
      result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
      if(result) {
        delete aToken;
        aToken=0;
      }
    } //if
  } //if
  return result;
}

/**
 *  This method is called just after we've consumed a start
 *  tag, and we now have to consume its attributes.
 *  
 *  @update  gpk 06/18/98
 *  @param   aChar: last char read
 *  @param   aScanner: see nsScanner.h
 *  @return  
 */
PRInt32 nsXIFDTD::ConsumeAttributes(PRUnichar aChar,CScanner& aScanner,CStartToken* aToken) {
  PRBool done=PR_FALSE;
  PRInt32 result=kNoError;
  nsAutoString as("");
  PRInt16 theAttrCount=0;

  while((!done) && (result==kNoError)) {
    CAttributeToken* theToken= new CAttributeToken(as);
    if(theToken){
      result=theToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    

      //Much as I hate to do this, here's some special case code.
      //This handles the case of empty-tags in XML. Our last
      //attribute token will come through with a text value of ""
      //and a textkey of "/". We should destroy it, and tell the 
      //start token it was empty.
      nsString& key=theToken->GetKey();
      nsString& text=theToken->GetText();
      if((key[0]==kForwardSlash) && (0==text.Length())){
        //tada! our special case! Treat it like an empty start tag...
        aToken->SetEmpty(PR_TRUE);
        delete theToken;
      }
      else  if(kNoError==result){
        theAttrCount++;
        mTokenDeque.Push(theToken);
      }//if
      else delete theToken; //we can't keep it...
    }//if
    
    if(kNoError==result){
      result=aScanner.Peek(aChar);
      if(aChar==kGreaterThan) { //you just ate the '>'
        aScanner.GetChar(aChar); //skip the '>'
        done=PR_TRUE;
      }//if
    }//if
  }//while

  aToken->SetAttributeCount(theAttrCount);
  return result;
}

/**
 *  This is a special case method. It's job is to consume 
 *  all of the given tag up to an including the end tag.
 *
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null
 */
PRInt32 nsXIFDTD::ConsumeContentToEndTag(const nsString& aString,PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  
  //In the case that we just read the given tag, we should go and
  //consume all the input until we find a matching end tag.

  nsAutoString endTag("</");
  endTag.Append(aString);
  endTag.Append(">");
  aToken=new CSkippedContentToken(endTag);
  return aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
}

/**
 *  This method is called just after a "<" has been consumed 
 *  and we know we're at the start of a tag.  
 *  
 *  @update gpk 06/18/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 nsXIFDTD::ConsumeStartTag(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  PRInt32 theDequeSize=mTokenDeque.GetSize();
  PRInt32 result=kNoError;

  aToken=new CStartToken(nsAutoString(""));

  if(aToken) {
    result= aToken->Consume(aChar,aScanner);  //tell new token to finish consuming text...    
    if(kNoError==result) {
      if(((CStartToken*)aToken)->IsAttributed()) {
        result=ConsumeAttributes(aChar,aScanner,(CStartToken*)aToken);
      }
    } //if
  } //if
  return result;
}

/**
 *  This method is called just after a "&" has been consumed 
 *  and we know we're at the start of an entity.  
 *  
 *  @update gpk 06/18/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 nsXIFDTD::ConsumeEntity(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
   PRUnichar  ch;
   PRInt32 result=aScanner.GetChar(ch);

   if(kNoError==result) {
     if(nsString::IsAlpha(ch)) { //handle common enity references &xxx; or &#000.
       aToken = new CEntityToken(nsAutoString(""));
       result = aToken->Consume(ch,aScanner);  //tell new token to finish consuming text...    
     }
     else if(kHashsign==ch) {
       aToken = new CEntityToken(nsAutoString(""));
       result=aToken->Consume(0,aScanner);
     }
     else {
       //oops, we're actually looking at plain text...
       nsAutoString temp("&");
       result=ConsumeText(temp,aScanner,aToken);
     }
   }//if
   return result;
}

/**
 *  This method is called just after whitespace has been 
 *  consumed and we know we're at the start a whitespace run.  
 *  
 *  @update gpk 06/18/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 nsXIFDTD::ConsumeWhitespace(PRUnichar aChar,CScanner& aScanner,CToken*& aToken) {
  aToken = new CWhitespaceToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
     result=aToken->Consume(aChar,aScanner);
  }
  return result;
}

/**
 *  This method is called just after a "<!" has been consumed 
 *  and we know we're at the start of a comment.  
 *  
 *  @update gpk 06/18/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 nsXIFDTD::ConsumeComment(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  aToken = new CCommentToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
     result=aToken->Consume(aChar,aScanner);
  }
  return result;
}

/**
 *  This method is called just after a known text char has
 *  been consumed and we should read a text run.
 *  
 *  @update gpk 06/18/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 nsXIFDTD::ConsumeText(const nsString& aString,CScanner& aScanner,CToken*& aToken){

  PRInt32 result=kNoError;
  if(aToken=new CTextToken(aString)) {
    PRUnichar ch=0;
    result=aToken->Consume(ch,aScanner);
  }
  return result;
}

/**
 *  This method is called just after a newline has been consumed. 
 *  
 *  @update gpk 06/18/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 nsXIFDTD::ConsumeNewline(PRUnichar aChar,CScanner& aScanner,CToken*& aToken){
  aToken=new CNewlineToken(nsAutoString(""));
  PRInt32 result=kNoError;
  if(aToken) {
    result=aToken->Consume(aChar,aScanner);
  }
  return result;
}

/**
 *  This method repeatedly called by the tokenizer. 
 *  Each time, we determine the kind of token were about to 
 *  read, and then we call the appropriate method to handle
 *  that token type.
 *  
 *  @update gpk 06/18/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 nsXIFDTD::ConsumeToken(CToken*& aToken){
  
  aToken=0;
  if(mTokenDeque.GetSize()>0) {
    aToken=(CToken*)mTokenDeque.Pop();
    return kNoError;
  }

  PRInt32   result=kNoError;
  CScanner* theScanner=mParser->GetScanner();
  if(kNoError==result){
    
    PRUnichar aChar;
    result=theScanner->GetChar(aChar);
    switch(result) {
      case kEOF:
        break;

      case kInterrupted:
        theScanner->RewindToMark();
        break; 

      case kNoError:
      default:
        switch(aChar) {
          case kLessThan:
            return ConsumeTag(aChar,*theScanner,aToken);

          case kAmpersand:
            return ConsumeEntity(aChar,*theScanner,aToken);
          
          case kCR: case kLF:
            return ConsumeNewline(aChar,*theScanner,aToken);
          
          case kNotFound:
            break;
          
          default:
            if(!nsString::IsSpace(aChar)) {
              nsAutoString temp(aChar);
              return ConsumeText(temp,*theScanner,aToken);
            }
            else return ConsumeWhitespace(aChar,*theScanner,aToken);
            break;
        } //switch
        break; 
    } //switch
    if(kNoError==result)
      result=theScanner->Eof();
  } //while
  return result;
}

/**
 * 
 * @update  gpk 06/18/98
 * @param 
 * @return
 */
CToken* nsXIFDTD::CreateTokenOfType(eHTMLTokenTypes aType) {
  return 0;
}


/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
void nsXIFDTD::WillResumeParse(void){
  if(mSink) {
    mSink->WillResume();
  }
  return;
}

/**
 * 
 * @update	gpk 06/18/98
 * @param 
 * @return
 */
void nsXIFDTD::WillInterruptParse(void){
  if(mSink) {
    mSink->WillInterrupt();
  }
  return;
}


/************************************************************************
  Here's a bunch of stuff JEvering put into the parser to do debugging.
 ************************************************************************/




/**
 * This debug method allows us to determine whether or not 
 * we've seen (and can handle) the given context vector.
 *
 * @update  gpk 06/18/98
 * @param   tags is an array of eXIFTags
 * @param   count represents the number of items in the tags array
 * @param   aDTD is the DTD we plan to ask for verification
 * @return  TRUE if we know how to handle it, else false
 */
PRBool nsXIFDTD::VerifyContextVector(void) const {

  PRBool  result=PR_TRUE;
  return result;
}

/**
 * This debug method allows us to determine whether or not 
 * we've seen (and can handle) the given context vector.
 *
 * @update  gpk 06/18/98
 * @param   tags is an array of eXIFTags
 * @param   count represents the number of items in the tags array
 * @param   aDTD is the DTD we plan to ask for verification
 * @return  TRUE if we know how to handle it, else false
 */
PRBool nsXIFDTD::Verify(const char* anOutputDir,PRBool aRecordStats) {

  PRBool  result=PR_TRUE;
  return result;
}



PRBool nsXIFDTD::CanContain(PRInt32 aParent,PRInt32 aChild) 
{
  return PR_TRUE;
}

void nsXIFDTD::SetURLRef(char * aURLRef)
{
}




/**
 * 
 * @update	jevering6/23/98
 * @param 
 * @return
 */

void nsXIFDTD::SetDTDDebug(nsIDTDDebug * aDTDDebug)
{
	if (mDTDDebug)
		NS_RELEASE(mDTDDebug);
	mDTDDebug = aDTDDebug;
	if (mDTDDebug)
		NS_ADDREF(mDTDDebug);
}




/*** CSS Methods ****/

void nsXIFDTD::BeginCSSStyleSheet(const nsIParserNode& aNode)
{
  nsString value;
  PRInt32  error;

  mBuffer.Truncate(0);
  mMaxCSSSelectorWidth = 10;
  if (GetAttribute(aNode,nsString("max_css_selector_width"),value))
  {
    PRInt32 temp = value.ToInteger(&error);
    if (error == NS_OK)
      mMaxCSSSelectorWidth = temp;
  }
  
  const char* name = NS_EnumToTag(eHTMLTag_html);
}

void nsXIFDTD::EndCSSStyleSheet(const nsIParserNode& aNode)
{
  const char* name = NS_EnumToTag(eHTMLTag_style);

  nsString tagName(name);

  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

  CStartToken   startToken(tagName);
  nsCParserNode startNode((CToken*)&startToken);

  mBuffer.Append("</");
  mBuffer.Append(tagName);
  mBuffer.Append(">");
  CSkippedContentToken* skipped = new CSkippedContentToken(mBuffer);
  
  startNode.SetSkippedContent(skipped);  
  mSink->AddLeaf(startNode);

}

void nsXIFDTD::BeginCSSStyleRule(const nsIParserNode& aNode)
{
  mCSSDeclarationCount = 0;
  mCSSSelectorCount = 0;  
}


void nsXIFDTD::EndCSSStyleRule(const nsIParserNode& aNode)
{
}


void nsXIFDTD::AddCSSSelector(const nsIParserNode& aNode)
{
  nsString value;

  if (mCSSSelectorCount > 0)
    mBuffer.Append(' ');

  
  if (GetAttribute(aNode, nsString("tag"), value))
  {
    if (mLowerCaseAttributes == PR_TRUE)
      value.ToLowerCase();
    else
      value.ToUpperCase();
    value.CompressWhitespace();
    mBuffer.Append(value);
  }

  if (GetAttribute(aNode,nsString("id"),value))
  {
    if (mLowerCaseAttributes == PR_TRUE)
      value.ToLowerCase();
    else
      value.ToUpperCase();
    value.CompressWhitespace();
    mBuffer.Append('#');
    mBuffer.Append(value);
  }
  
  if (GetAttribute(aNode,nsString("class"),value))
  {
    if (mLowerCaseAttributes == PR_TRUE)
      value.ToLowerCase();
    else
      value.ToUpperCase();

    value.CompressWhitespace();
    mBuffer.Append('.');
    mBuffer.Append(value);
  }

  if (GetAttribute(aNode,nsString("pseudo_class"),value))
  {
    if (mLowerCaseAttributes == PR_TRUE)
      value.ToLowerCase();
    else
      value.ToUpperCase();

    value.CompressWhitespace();
    mBuffer.Append(':');
    mBuffer.Append(value);
  }
  mCSSSelectorCount++;
}

void nsXIFDTD::BeginCSSDeclarationList(const nsIParserNode& aNode)
{
  PRInt32 index = mBuffer.RFind('\n');
  if (index == kNotFound)
    index = 0;
  PRInt32 offset = mBuffer.Length() - index;
  PRInt32 count = mMaxCSSSelectorWidth - offset;

  if (count < 0)
    count = 0;

  for (PRInt32 i = 0; i < count; i++)
    mBuffer.Append(" ");

  mBuffer.Append("   {");
  mCSSDeclarationCount = 0;

}

void nsXIFDTD::EndCSSDeclarationList(const nsIParserNode& aNode)
{
  mBuffer.Append("}\n");
}


void nsXIFDTD::AddCSSDeclaration(const nsIParserNode& aNode)
{
  nsString property;
  nsString value;


  if (PR_TRUE == GetAttribute(aNode, nsString("property"), property))
    if (PR_TRUE == GetAttribute(aNode, nsString("value"), value))
    {
      if (mCSSDeclarationCount != 0)
        mBuffer.Append(";");
      mBuffer.Append(" ");
      mBuffer.Append(property);
      mBuffer.Append(": ");
      mBuffer.Append(value);
      mCSSDeclarationCount++;
    }
}


