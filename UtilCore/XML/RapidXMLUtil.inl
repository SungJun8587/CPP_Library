
//***************************************************************************
// @brief 대입 연산자(=) 오버로딩 (노드에 값 할당)
// @param value 할당할 값 (정수, 실수, 문자열 등)
// @return CRapidXMLUtil 객체 참조
// @detail 문서에 아직 노드가 하나도 없는 상태(예: AddNode를 아직 호출하지 않은 상태)에서
//         호출되면 대입할 대상 노드가 없으므로 아무 동작도 하지 않고 그대로 반환합니다.
//***************************************************************************
template <typename T>
inline const CRapidXMLUtil& CRapidXMLUtil::operator=(const T& value)
{
    rapidxml::xml_node<char>* firstNode = _doc.first_node();
    if( !firstNode ) return *this;

    rapidxml::xml_node<char>* node = firstNode->last_node();
    if( !node ) return *this;

    if constexpr( std::is_arithmetic<T>::value )
    {
        node->value(_doc.allocate_string(std::to_string(value).c_str()));
    }
    else if constexpr( std::is_same_v<T, _tstring> )
    {
        node->value(_doc.allocate_string(TStringToUtf8(value).c_str()));
    }
    else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
    {
        node->value(_doc.allocate_string(TStringToUtf8(value).c_str()));
    }

    return *this;
}

//***************************************************************************
// @brief 객체를 XML 형태로 파일에 저장합니다.
// @param obj 저장할 객체
// @param filename 저장할 파일 경로
// @return 성공 시 true, 실패 시 false
//***************************************************************************
template <typename T>
inline bool CRapidXMLUtil::SaveToFile(const T& obj, const _tstring& filename)
{
    std::string xmlContent = Serialize(obj);
    std::ofstream file(filename);
    if( !file.is_open() )
    {
        _tcerr << _T("Failed to open file for writing:") << filename << std::endl;
        return false;
    }

    file << xmlContent;
    file.close();

    return true;
}

//***************************************************************************
// @brief 파일에서 XML 데이터를 읽어 객체로 역직렬화합니다.
// @param filename 읽을 파일 경로
// @return 역직렬화된 객체 T (파일을 열 수 없으면 기본 생성된 T를 반환)
//***************************************************************************
template <typename T>
inline T CRapidXMLUtil::LoadFromFile(const _tstring& filename)
{
    std::ifstream file(filename);
    if( !file.is_open() )
    {
        _tcerr << _T("Failed to open file for reading:") << filename << std::endl;
        return T{};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return Deserialize<T>(buffer.str());
}

//***************************************************************************
// @brief 특정 이름을 가진 노드를 추가하고 객체를 XML로 변환합니다.
// @param nodeName 노드 이름
// @param obj 변환 및 추가할 객체
//***************************************************************************
template <typename T>
void CRapidXMLUtil::AddNode(const _tstring& nodeName, const T& obj)
{
    BuildXMLNode(nodeName, obj);
}

//***************************************************************************
// @brief 기존 노드를 삭제하고 새로운 객체 정보로 노드를 갱신합니다.
// @param nodeName 갱신할 노드 이름
// @param obj 갱신할 객체
//***************************************************************************
template <typename T>
void CRapidXMLUtil::UpdateNode(const _tstring& nodeName, const T& obj)
{
    // 기존 노드 삭제하고 다시 추가
    RemoveNode(nodeName);
    BuildXMLNode(nodeName, obj);
}

//***************************************************************************
// @brief 객체를 들여쓰기(Indent)가 포함된 XML 문자열로 직렬화합니다.
// @param obj 직렬화할 객체
// @return 직렬화된 UTF-8 XML 문자열 (들여쓰기 포함)
//***************************************************************************
template <typename T>
inline std::string CRapidXMLUtil::SerializeWithIndent(const T& obj)
{
    _doc.clear();
    xml_node<>* root = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(RootName).c_str()));
    _doc.append_node(root);

    // ConvertToXML의 dispatch 로직을 재사용해 obj를 문서 트리에 채워 넣습니다.
    ConvertToXML(_T(""), obj);

    std::string xmlContent;
    print(std::back_inserter(xmlContent), _doc, 0);   // flags = 0 : 들여쓰기 포함 출력
    return xmlContent;
}

//***************************************************************************
// @brief 객체를 압축된(들여쓰기 없는) XML 문자열로 직렬화합니다.
// @param obj 직렬화할 객체
// @return 직렬화된 UTF-8 XML 문자열 (압축 형태)
//***************************************************************************
template <typename T>
inline std::string CRapidXMLUtil::Serialize(const T& obj)
{
    _doc.clear();
    xml_node<>* root = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(RootName).c_str()));
    _doc.append_node(root);

    // ConvertToXML(...)을 거치면 내부적으로 만든 UTF-8 문자열을 다시 _tstring으로
    // 변환했다가 여기서 또 UTF-8로 되돌리는 불필요한 왕복 변환이 발생하므로,
    // 트리만 구성하는 BuildXMLNode를 직접 호출해 왕복 변환을 피합니다.
    BuildXMLNode(_T(""), obj);

    std::string xmlContent;
    print(std::back_inserter(xmlContent), _doc, print_no_indenting);
    return xmlContent;
}

//***************************************************************************
// @brief XML 문자열을 객체로 역직렬화합니다.
// @param xml 역직렬화할 XML 문자열 (UTF-8)
// @return 역직렬화된 객체 T (파싱 실패 또는 Root 노드가 없으면 기본 생성된 T를 반환)
//***************************************************************************
template <typename T>
inline T CRapidXMLUtil::Deserialize(const std::string& xml)
{
    T obj{};
    _doc.clear();

    if( xml.empty() ) return obj;

    CVector<char> buffer(xml.begin(), xml.end());
    buffer.push_back('\0');

    try
    {
        _doc.parse<0>(&buffer[0]);
    }
    catch( const parse_error& e )
    {
        _tcerr << _T("XML parse error : ") << e.what() << std::endl;
        _tcerr << _T("Location of error : ") << e.where<TCHAR>() << std::endl;
        return obj;
    }

    xml_node<>* root = _doc.first_node(TStringToUtf8(RootName).c_str());
    if( !root )
    {
        _tcerr << _T("Root node not found in XML") << std::endl;
        return obj;
    }

    std::optional<T> result = ConvertFromXML<T>(_T(""));
    return result.has_value() ? result.value() : obj;
}

//***************************************************************************
// @brief 사용자 정의 객체를 XML 노드로 추가(직렬화)합니다.
// @param obj 추가할 사용자 정의 객체
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename T>
inline void CRapidXMLUtil::AddObject(const T& obj, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* classNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(classNode);
    obj.ToXML(classNode, _doc);
}

//***************************************************************************
// @brief XML 노드로부터 사용자 정의 객체 정보를 읽어옵니다(역직렬화).
// @param obj [OUT] 읽어온 정보를 저장할 객체 참조
// @param node 대상 XML 노드 포인터
//***************************************************************************
template <typename T>
inline void CRapidXMLUtil::GetObject(T& obj, xml_node<>* node)
{
    if( node )
    {
        obj.FromXML(node);
    }
}

//***************************************************************************
// @brief 기본 데이터 벡터(CVector<T> 또는 std::vector<T>)를 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 벡터 컨테이너
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename Container, typename ValueType>
inline void CRapidXMLUtil::AddVector(const Container& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        if constexpr( std::is_arithmetic<ValueType>::value )
        {
            AddValue(item, containerNode);
        }
        else if constexpr( std::is_same_v<ValueType, _tstring> )
        {
            AddValue(item, containerNode);
        }
    }
}

//***************************************************************************
// @brief XML 노드로부터 기본 데이터 벡터(CVector<T> 또는 std::vector<T>)를 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 벡터 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename Container, typename ValueType>
inline void CRapidXMLUtil::GetVector(Container& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( !containerNode ) return;

    // 반복마다 매번 변환하지 않도록 태그 이름은 루프 진입 전에 한 번만 UTF-8로 변환합니다.
    std::string itemTag = TStringToUtf8(ItemName);
    for( xml_node<>* node = containerNode->first_node(itemTag.c_str()); node; node = node->next_sibling(itemTag.c_str()) )
    {
        ValueType item{};

        if constexpr( std::is_arithmetic<ValueType>::value )
        {
            GetValue(item, node);
        }
        else if constexpr( std::is_same_v<ValueType, _tstring> )
        {
            GetValue(item, node);
        }

        container.push_back(item);
    }
}

//***************************************************************************
// @brief 사용자 정의 객체 벡터(CVector<T> 또는 std::vector<T>)를 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 객체 벡터 컨테이너
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename Container, typename ValueType>
inline void CRapidXMLUtil::AddObjectVector(const Container& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    std::string itemTagUtf8 = TStringToUtf8(ItemName);
    for( const auto& item : container )
    {
        xml_node<>* classNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(itemTagUtf8.c_str()));
        containerNode->append_node(classNode);
        item.ToXML(classNode, _doc);
    }
}

//***************************************************************************
// @brief XML 노드로부터 사용자 정의 객체 벡터(CVector<T> 또는 std::vector<T>)를 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 객체 벡터 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename Container, typename ValueType>
inline void CRapidXMLUtil::GetObjectVector(Container& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( !containerNode ) return;

    std::string itemTag = TStringToUtf8(ItemName);
    for( xml_node<>* node = containerNode->first_node(itemTag.c_str()); node; node = node->next_sibling(itemTag.c_str()) )
    {
        ValueType item;
        item.FromXML(node);
        container.push_back(item);
    }
}

//***************************************************************************
// @brief 기본 데이터 맵(CMap<K,V> 또는 std::map<K,V>)을 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 맵 컨테이너 (키는 _tstring만 지원)
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename MapContainer>
inline void CRapidXMLUtil::AddMap(const MapContainer& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    using KeyType = typename MapContainer::key_type;
    using ValueType = typename MapContainer::mapped_type;

    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    std::string itemTagUtf8 = TStringToUtf8(ItemName);
    for( const auto& item : container )
    {
        xml_node<>* itemNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(itemTagUtf8.c_str()));
        containerNode->append_node(itemNode);

        if constexpr( std::is_arithmetic<KeyType>::value || std::is_same_v<KeyType, _tstring> )
        {
            AddValue(item.first, itemNode, MapKey);       // 키 직렬화
        }

        if constexpr( std::is_arithmetic<ValueType>::value || std::is_same_v<ValueType, _tstring> )
        {
            AddValue(item.second, itemNode, MapValue);    // 값 직렬화
        }
    }
}

//***************************************************************************
// @brief XML 노드로부터 기본 데이터 맵(CMap<K,V> 또는 std::map<K,V>)을 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 맵 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename MapContainer>
inline void CRapidXMLUtil::GetMap(MapContainer& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    using KeyType = typename MapContainer::key_type;
    using ValueType = typename MapContainer::mapped_type;

    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( !containerNode ) return;

    std::string itemTag = TStringToUtf8(ItemName);
    std::string keyTag = TStringToUtf8(MapKey);
    std::string valueTag = TStringToUtf8(MapValue);

    for( xml_node<>* node = containerNode->first_node(itemTag.c_str()); node; node = node->next_sibling(itemTag.c_str()) )
    {
        KeyType key{};
        ValueType value{};

        xml_node<>* keyNode = node->first_node(keyTag.c_str());
        if( keyNode )
        {
            if constexpr( std::is_arithmetic<KeyType>::value || std::is_same_v<KeyType, _tstring> )
            {
                GetValue(key, keyNode);
            }
        }

        xml_node<>* valueNode = node->first_node(valueTag.c_str());
        if( valueNode )
        {
            if constexpr( std::is_arithmetic<ValueType>::value || std::is_same_v<ValueType, _tstring> )
            {
                GetValue(value, valueNode);
            }
        }

        container[key] = value;
    }
}

//***************************************************************************
// @brief 사용자 정의 객체 맵(CMap<K,V> 또는 std::map<K,V>)을 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 객체 맵 컨테이너 (키는 _tstring만 지원)
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename MapContainer>
inline void CRapidXMLUtil::AddObjectMap(const MapContainer& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    std::string itemTagUtf8 = TStringToUtf8(ItemName);
    for( const auto& item : container )
    {
        xml_node<>* itemNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(itemTagUtf8.c_str()));
        containerNode->append_node(itemNode);

        xml_node<>* node = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(MapKey).c_str()), _doc.allocate_string(TStringToUtf8(item.first).c_str()));
        itemNode->append_node(node);

        xml_node<>* classNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(MapValue).c_str()));
        itemNode->append_node(classNode);
        item.second.ToXML(classNode, _doc);
    }
}

//***************************************************************************
// @brief XML 노드로부터 사용자 정의 객체 맵(CMap<K,V> 또는 std::map<K,V>)을 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 객체 맵 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename MapContainer>
inline void CRapidXMLUtil::GetObjectMap(MapContainer& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    using KeyType = typename MapContainer::key_type;
    using ValueType = typename MapContainer::mapped_type;

    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( !containerNode ) return;

    std::string itemTag = TStringToUtf8(ItemName);
    std::string keyTag = TStringToUtf8(MapKey);
    std::string valueTag = TStringToUtf8(MapValue);

    for( xml_node<>* node = containerNode->first_node(itemTag.c_str()); node; node = node->next_sibling(itemTag.c_str()) )
    {
        KeyType key{};
        ValueType value{};

        xml_node<>* keyNode = node->first_node(keyTag.c_str());
        if( keyNode )
        {
            key = Utf8ToTString(keyNode->value());
        }

        xml_node<>* valueNode = node->first_node(valueTag.c_str());
        if( valueNode )
        {
            value.FromXML(valueNode);
        }

        container[key] = value;
    }
}

//***************************************************************************
// @brief 숫자 타입 값을 XML 노드로 추가(직렬화)합니다.
// @param value 추가할 숫자 값
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type*>
inline void CRapidXMLUtil::AddValue(const T& value, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* node = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()), _doc.allocate_string(std::to_string(value).c_str()));
    parent->append_node(node);
}

//***************************************************************************
// @brief XML 노드로부터 숫자 타입 값을 읽어옵니다(역직렬화).
// @param value [OUT] 읽어온 값을 저장할 숫자 변수 참조
// @param node 대상 XML 노드 포인터
//***************************************************************************
template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type*>
inline void CRapidXMLUtil::GetValue(T& value, xml_node<>* node)
{
    // std::stod는 (1) 값이 빈 문자열이면 예외를 던지고 (2) 현재 전역 C 로케일
    // (setlocale로 변경될 수 있는)의 영향을 받습니다. _strtod_l은 실패 시 예외 없이
    // 0.0을 반환하고, 항상 고정된 "C" 로케일로 파싱하므로 벡터/맵 항목 파싱에 더 안전합니다.
    if( node && node->value() )
        value = static_cast<T>(_strtod_l(node->value(), nullptr, g_xmlNumericLocale));
}

//***************************************************************************
// @brief 다양한 타입의 객체/컨테이너를 문서 트리에 채워 넣습니다 (직렬화 없이).
// @param nodeName 노드 이름
// @param obj 변환할 객체
//***************************************************************************
template <typename T>
inline void CRapidXMLUtil::BuildXMLNode(const _tstring& nodeName, const T& obj)
{
    xml_node<char>* root = _doc.first_node();
    if( !root )
    {
        root = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(RootName).c_str()));
        _doc.append_node(root);
    }

    if constexpr( is_vector<T>::value )
    {
        using ValueType = typename T::value_type;
        if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
        {
            AddVector(obj, root, nodeName.size() > 0 ? nodeName.c_str() : VectorName);        // 기본 자료형 벡터
        }
        else
        {
            AddObjectVector(obj, root, nodeName.size() > 0 ? nodeName.c_str() : VectorName);  // 사용자 정의 클래스 벡터
        }
    }
    else if constexpr( is_map<T>::value )
    {
        using KeyType = typename T::key_type;
        using ValueType = typename T::mapped_type;

        if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
        {
            // 기본 자료형 값을 갖는 맵: AddMap/GetMap은 키로 _tstring뿐 아니라
            // 산술 타입도 지원하므로(예: std::map<int, double>), 그에 맞춰 허용합니다.
            if constexpr( std::is_same_v<KeyType, _tstring> || std::is_arithmetic_v<KeyType> )
            {
                AddMap(obj, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);
            }
            else
            {
                static_assert(dependent_false<T>::value, "CRapidXMLUtil: map key type must be _tstring or an arithmetic type");
            }
        }
        else
        {
            // 사용자 정의 클래스를 값으로 갖는 맵: AddObjectMap/GetObjectMap은 키를
            // _tstring으로만 다루므로(문자열 태그로 저장) 키 타입을 그렇게 제한합니다.
            if constexpr( std::is_same_v<KeyType, _tstring> )
            {
                AddObjectMap(obj, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);
            }
            else
            {
                static_assert(dependent_false<T>::value, "CRapidXMLUtil: object-value map key type must be _tstring");
            }
        }
    }
    else if constexpr( has_toxml_method<T>::value )
    {
        AddObject(obj, root, nodeName.size() > 0 ? nodeName.c_str() : ItemName);
    }
    else if constexpr( std::is_arithmetic<T>::value )
    {
        AddValue(obj, root, nodeName.size() > 0 ? nodeName.c_str() : ItemName);
    }
    else if constexpr( std::is_same_v<T, _tstring> )
    {
        AddValue(obj, root, nodeName.size() > 0 ? nodeName.c_str() : ItemName);
    }
    else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
    {
        AddValue(obj, root, nodeName.size() > 0 ? nodeName.c_str() : ItemName);
    }
}

//***************************************************************************
// @brief 다양한 타입의 객체/컨테이너를 XML 구조로 변환하여 문자열로 반환합니다.
// @param nodeName 노드 이름
// @param obj 변환할 객체
// @return 변환된 XML 문자열 (_tstring, 압축 형태)
// @detail 트리 구성은 BuildXMLNode에 위임하고, 문자열 결과가 필요한 이 함수에서만
//         전체 문서를 1회 직렬화합니다. 문자열 결과가 필요 없는 AddNode/UpdateNode/
//         Proxy::operator=/Serialize 등은 BuildXMLNode를 직접 호출해 이 직렬화 비용을 피합니다.
//***************************************************************************
template <typename T>
inline _tstring CRapidXMLUtil::ConvertToXML(const _tstring& nodeName, const T& obj)
{
    BuildXMLNode(nodeName, obj);

    std::string xmlContent;
    print(std::back_inserter(xmlContent), _doc, print_no_indenting);
    return Utf8ToTString(xmlContent);
}

//***************************************************************************
// @brief XML 노드로부터 지정한 타입 T의 객체/컨테이너 값을 가져옵니다.
// @param nodeName 노드 이름
// @return 변환 성공 시 std::optional<T>, 실패 시 std::nullopt
//***************************************************************************
template <typename T>
inline std::optional<T> CRapidXMLUtil::ConvertFromXML(const _tstring& nodeName)
{
    // T가 산술 타입일 때 "T value;"는 초기화되지 않은(indeterminate) 값을 가지므로,
    // 대응하는 XML 노드를 찾지 못해 GetValue가 값을 채우지 못하는 경우 호출자에게
    // 쓰레기 값이 반환될 수 있습니다. 값 초기화로 항상 0/빈 상태에서 시작합니다.
    T value{};

    xml_node<char>* root = _doc.first_node();
    if( !root ) return std::nullopt;        // 노드가 없거나 값이 없으면 std::nullopt 반환

    if constexpr( is_vector<T>::value )
    {
        using ValueType = typename T::value_type;
        if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
        {
            GetVector<T, ValueType>(value, root, nodeName.size() > 0 ? nodeName.c_str() : VectorName);        // 기본 자료형 벡터
        }
        else
        {
            GetObjectVector<T, ValueType>(value, root, nodeName.size() > 0 ? nodeName.c_str() : VectorName);  // 사용자 정의 클래스 벡터
        }
    }
    else if constexpr( is_map<T>::value )
    {
        using KeyType = typename T::key_type;
        using ValueType = typename T::mapped_type;

        if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
        {
            // 기본 자료형 값을 갖는 맵: 키는 _tstring 또는 산술 타입을 지원합니다 (AddMap/GetMap과 동일 기준).
            if constexpr( std::is_same_v<KeyType, _tstring> || std::is_arithmetic_v<KeyType> )
            {
                GetMap<T>(value, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);
            }
            else
            {
                static_assert(dependent_false<T>::value, "CRapidXMLUtil: map key type must be _tstring or an arithmetic type");
            }
        }
        else
        {
            // 사용자 정의 클래스를 값으로 갖는 맵: 키는 _tstring만 지원합니다.
            if constexpr( std::is_same_v<KeyType, _tstring> )
            {
                GetObjectMap<T>(value, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);
            }
            else
            {
                static_assert(dependent_false<T>::value, "CRapidXMLUtil: object-value map key type must be _tstring");
            }
        }
    }
    else if constexpr( has_toxml_method<T>::value )
    {
        GetObject<T>(value, root->first_node(nodeName.size() > 0 ? TStringToUtf8(nodeName).c_str() : nullptr));
    }
    else if constexpr( std::is_same_v<T, _tstring> )
    {
        GetValue(value, root->first_node(nodeName.size() > 0 ? TStringToUtf8(nodeName).c_str() : nullptr));
    }
    else if constexpr( std::is_arithmetic<T>::value )
    {
        GetValue(value, root->first_node(nodeName.size() > 0 ? TStringToUtf8(nodeName).c_str() : nullptr));
    }

    return optional<T>(value);
}
