class nsGetServiceByContractID {
};

template <class T>
struct already_AddRefed
{
  already_AddRefed( T* aRawPtr )
      : mRawPtr(aRawPtr)
    {
      // nothing else to do here
    }

  T* mRawPtr;
};
  
template <class T>
class nsDerivedSafe : public T {
};

template<typename T> class nsCOMPtr {
  T* pointer;
public:
  nsCOMPtr();
  nsCOMPtr(T*);
  nsCOMPtr(nsGetServiceByContractID c);
  nsCOMPtr(const already_AddRefed<T>& c);
  operator nsDerivedSafe<T>*() const;
  nsDerivedSafe<T>* operator->() const;
  void
  swap( nsCOMPtr<T>& rhs )
  // ...exchange ownership with |rhs|; can save a pair of refcount operations
  {
    T* temp = rhs.pointer;
    rhs.pointer = pointer;
    pointer = temp;
  }
 
  T*
  get() const
  {
    return pointer;
  }
  T* forget();
};
template <class T>
inline
nsCOMPtr<T>*
address_of( nsCOMPtr<T>& aPtr )
  {
    return 0;
  }

//new func to be used for swapping ex-COMPtrs
template <class T> void swap(T* &a, T* &b);
template <class T>
class nsGetterAddRefs
{
public:
  nsGetterAddRefs( nsCOMPtr<T>& aSmartPtr );
  operator T**();
  operator void**();
  T*& operator*();
};

template <class T> nsGetterAddRefs<T> getter_AddRefs( nsCOMPtr<T>& aSmartPtr );

class nsIID;
class nsIInterfaceRequestor {
public:
  int GetInterface(const nsIID  &iid, void **result);
};

class nsICSSStyleSheet {
};
class nsIURI;

typedef int PRBool;
class nsIXULPrototypeCache {
public:
  int GetEnabled(PRBool *enabled);
  int GetStyleSheet(nsIURI* aURI, nsICSSStyleSheet** _result);
  int PutStyleSheet(nsICSSStyleSheet* aStyleSheet);
};
class LoadData {
  public:
    //for testing only
    LoadData(nsGetServiceByContractID c);
    nsICSSStyleSheet* mSheet;
    nsIURI* mURI;
};
const int NS_OK = 0;

template<class T> void test_templ(nsCOMPtr<T> &templ_outparam, const nsIID  &iid) {
  nsCOMPtr<nsIInterfaceRequestor> cbs;
  cbs->GetInterface(iid, getter_AddRefs(templ_outparam));
}

const nsGetServiceByContractID do_GetService(const char* aContractID);
class Bah {
public:
  already_AddRefed<nsIXULPrototypeCache> foobu( nsCOMPtr<nsIXULPrototypeCache> &cbs);
  nsCOMPtr<nsIXULPrototypeCache> foobuok();
  void testArrayParams(nsCOMPtr<nsIXULPrototypeCache> bah);
  nsCOMPtr<nsIXULPrototypeCache> goo(nsIXULPrototypeCache **, nsCOMPtr<nsIXULPrototypeCache> *) {
    // the first param not get rewritten
    // second should
    nsCOMPtr <nsIXULPrototypeCache> beh = foobuok();
    nsIID *iid;
    test_templ(mProtoCache, *iid);
    goo(getter_AddRefs(mProtoCache), &mProtoCache);
    return goo(getter_AddRefs(mProtoCache), address_of(mProtoCache));
  }

  nsCOMPtr<nsIXULPrototypeCache> mProtoCache;
};

already_AddRefed<nsIXULPrototypeCache> foo(nsIXULPrototypeCache *cbs);


already_AddRefed<nsIXULPrototypeCache> foobu( nsCOMPtr<nsIXULPrototypeCache> &cbs,
					      nsCOMPtr<nsIXULPrototypeCache> *cbs2) {
  nsIXULPrototypeCache *c = 0;
  return c;
}



void test(LoadData *aLoadData, Bah *test) {
  nsCOMPtr<nsIXULPrototypeCache> cache, bleh;
  bleh = test->foobu(cache);
  bleh = foobu(cache, address_of(cache));
  bleh.swap(cache);

  nsIXULPrototypeCache *c = bleh.get();
  c = bleh.forget();
  if (cache) {
    PRBool enabled;
    cache->GetEnabled(&enabled);
    if (enabled) {
      nsCOMPtr<nsICSSStyleSheet> sheet;
      cache->GetStyleSheet(aLoadData->mURI, getter_AddRefs(sheet));
      if (!sheet) {
        cache->PutStyleSheet(aLoadData->mSheet);
      }
    }
  }
}
#if 0
void foeo();

void bleh() {
  foeo();
}

void foeo() {
}

class nsILoadGroup {
public:
  int GetNotificationCallbacks(nsIInterfaceRequestor**);
};
class nsIChannel {
public:
  int GetNotificationCallbacks(nsIInterfaceRequestor**);
  int GetLoadGroup(nsILoadGroup**);
};

class ConstructorRefs {
public:
  ConstructorRefs(nsCOMPtr<nsIInterfaceRequestor>* blacklistme):
    mPtr(blacklistme) {
  }
  ConstructorRefs(nsCOMPtr<nsIInterfaceRequestor>* blacklistme2, int);
  nsCOMPtr<nsIInterfaceRequestor> *mPtr;
};


class __attribute__((stack)) StackClass
{
 public:
  StackClass() :mFoo3(0) {}
  StackClass(int i) : mI(i) { }
  StackClass(char*) : mFoo(0) { }
  ~StackClass() {}

  int mI;
  nsCOMPtr<nsIInterfaceRequestor> mFoo;
  nsCOMPtr<nsIInterfaceRequestor> *mPointer;
  nsCOMPtr<nsIInterfaceRequestor> mFoo2;
  nsCOMPtr<nsIInterfaceRequestor> mFoo3;
};

// this has a nice transitive example
int test2(nsIChannel   *channel, const nsIID  &iid, void **result) {
    *result = 0;
    nsCOMPtr<nsIInterfaceRequestor> cbs;
    nsCOMPtr<nsIInterfaceRequestor> cbs2;
    test_templ(cbs, iid);
    channel->GetNotificationCallbacks(nsGetterAddRefs<nsIInterfaceRequestor>(cbs));
    cbs->GetInterface(iid, nsGetterAddRefs<nsIInterfaceRequestor>(cbs2));
    StackClass sc;
    channel->GetNotificationCallbacks(getter_AddRefs(sc.mFoo));
    if (cbs)
        cbs->GetInterface(iid, result);
    if (!*result) {
        // try load group's notification callbacks...
        nsCOMPtr<nsILoadGroup> loadGroup;
	test_templ(loadGroup, iid);
        channel->GetLoadGroup(getter_AddRefs(loadGroup));
        if (loadGroup) {
            loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
            if (cbs)
                cbs->GetInterface(iid, result);
        }
    }
    foeo();
}

int testPointers(void *foo) {
  nsCOMPtr<nsIInterfaceRequestor> *p1 = static_cast<nsCOMPtr<nsIInterfaceRequestor>*>(foo);
  StackClass sc;
  *p1 = sc.mFoo2;
  p1 = sc.mPointer;
  p1 = (nsCOMPtr<nsIInterfaceRequestor>*) foo;
}

#endif
