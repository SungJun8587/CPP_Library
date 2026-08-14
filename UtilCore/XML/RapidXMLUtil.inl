//***************************************************************************
// @brief 대입 연산자(=) 오버로딩 (노드에 값 할당)
// @param value 할당할 값 (정수, 실수, 문자열 등)
// @return CRapidXMLUtil 객체 참조
//***************************************************************************
template <typename T>
inline const CRapidXMLUtil& CRapidXMLUtil::operator=(const T& value)
{
    rapidxml::xml_node<char>* node = _doc.first_node()->last_node();

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
// @return 역직렬화된 객체 T
//***************************************************************************
template <typename T>
inline T CRapidXMLUtil::LoadFromFile(const _tstring& filename)
{
    std::ifstream file(filename);
    if( !file.is_open() )
    {
        _tcerr << _T("Failed to open file for reading:") << filename << std::endl;
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
    ConvertToXML(nodeName, obj);
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
    ConvertToXML(nodeName, obj);
}

//***************************************************************************
// @brief 객체를 들여쓰기(Indent)가 포함된 XML 문자열로 직렬화합니다.
// @param obj 직렬화할 객체
// @return 직렬화된 UTF-8 XML 문자열
//***************************************************************************
template <typename T>
inline std::string CRapidXMLUtil::SerializeWithIndent(const T& obj)
{
    _doc.clear();
    xml_node<>* root = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(RootName).c_str()));
    _doc.append_node(root);

    SerializeObject(obj, root);

    std::string xmlContent;
    print(std::back_inserter(xmlContent), _doc, 0);
    return xmlContent;
}

//***************************************************************************
// @brief 객체를 XML 문자열로 직렬화합니다.
// @param obj 직렬화할 객체
// @return 직렬화된 UTF-8 XML 문자열
//***************************************************************************
template <typename T>
inline std::string CRapidXMLUtil::Serialize(const T& obj)
{
    _doc.clear();
    xml_node<>* root = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(RootName).c_str()));
    _doc.append_node(root);

    return TStringToUtf8(ConvertToXML(_T(""), obj));
}

//***************************************************************************
// @brief XML 문자열을 객체로 역직렬화합니다.
// @param xml 역직렬화할 XML 문자열 (UTF-8)
// @return 역직렬화된 객체 T
//***************************************************************************
template <typename T>
inline T CRapidXMLUtil::Deserialize(const std::string& xml)
{
    T obj;
    _doc.clear();
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
    }

    xml_node<>* root = _doc.first_node(_doc.allocate_string(TStringToUtf8(RootName).c_str()));
    if( !root )
    {
        _tcerr << _T("Root node not found in XML") << std::endl;
    }

    return ConvertFromXML<T>(_T("")).value();
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
// @brief 기본 데이터 벡터(CVector)를 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 벡터 컨테이너
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename T>
inline void CRapidXMLUtil::AddVector(const CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        if constexpr( std::is_arithmetic<T>::value )
        {
            AddValue(item, containerNode);
        }
        else if constexpr( std::is_same_v<T, _tstring> )
        {
            AddValue(item, containerNode);
        }
    }
}

//***************************************************************************
// @brief XML 노드로부터 기본 데이터 벡터(CVector)를 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 벡터 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename T>
inline void CRapidXMLUtil::GetVector(CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( containerNode )
    {
        for( xml_node<>* node = containerNode->first_node(TStringToUtf8(ItemName).c_str()); node; node = node->next_sibling(TStringToUtf8(ItemName).c_str()) )
        {
            T item;

            if constexpr( std::is_arithmetic<T>::value )
            {
                GetValue(item, node);
            }
            else if constexpr( std::is_same_v<T, _tstring> )
            {
                GetValue(item, node);
            }

            container.push_back(item);
        }
    }
}

//***************************************************************************
// @brief 사용자 정의 객체 벡터(CVector)를 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 객체 벡터 컨테이너
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename T>
inline void CRapidXMLUtil::AddObjectVector(const CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        xml_node<>* classNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ItemName).c_str()));
        containerNode->append_node(classNode);
        item.ToXML(classNode, _doc);
    }
}

//***************************************************************************
// @brief XML 노드로부터 사용자 정의 객체 벡터(CVector)를 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 객체 벡터 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename T>
inline void CRapidXMLUtil::GetObjectVector(CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( containerNode )
    {
        for( xml_node<>* node = containerNode->first_node(TStringToUtf8(ItemName).c_str()); node; node = node->next_sibling(TStringToUtf8(ItemName).c_str()) )
        {
            T item;
            item.FromXML(node);
            container.push_back(item);
        }
    }
}

//***************************************************************************
// @brief 기본 데이터 맵(CMap)을 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 맵 컨테이너
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename K, typename V>
inline void CRapidXMLUtil::AddMap(const CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        xml_node<>* itemNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ItemName).c_str()));
        containerNode->append_node(itemNode);

        if constexpr( std::is_arithmetic<K>::value )
        {
            AddValue(item.first, itemNode, MapKey);       // 숫자 타입 키 직렬화
        }
        else if constexpr( std::is_same_v<K, _tstring> )
        {
            AddValue(item.first, itemNode, MapKey);       // 문자열 타입 키 직렬화
        }

        if constexpr( std::is_arithmetic<V>::value )
        {
            AddValue(item.second, itemNode, MapValue);    // 숫자 타입 값 직렬화
        }
        else if constexpr( std::is_same_v<V, _tstring> )
        {
            AddValue(item.second, itemNode, MapValue);    // 문자열 타입 값 직렬화
        }
    }
}

//***************************************************************************
// @brief XML 노드로부터 기본 데이터 맵(CMap)을 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 맵 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename K, typename V>
inline void CRapidXMLUtil::GetMap(CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( containerNode )
    {
        for( xml_node<>* node = containerNode->first_node(TStringToUtf8(ItemName).c_str()); node; node = node->next_sibling(TStringToUtf8(ItemName).c_str()) )
        {
            K key;
            V value;

            xml_node<>* keyNode = node->first_node(TStringToUtf8(MapKey).c_str());
            if( keyNode )
            {
                if constexpr( std::is_arithmetic<K>::value )
                {
                    GetValue(key, keyNode);    // 숫자 타입 키 역직렬화
                }
                else if constexpr( std::is_same_v<K, _tstring> )
                {
                    GetValue(key, keyNode);    // 문자열 타입 키 역직렬화
                }
            }

            xml_node<>* valueNode = node->first_node(TStringToUtf8(MapValue).c_str());
            if( valueNode )
            {
                if constexpr( std::is_arithmetic<V>::value )
                {
                    GetValue(value, valueNode);    // 숫자 타입 값 역직렬화
                }
                else if constexpr( std::is_same_v<V, _tstring> )
                {
                    GetValue(value, valueNode);    // 문자열 타입 값 역직렬화
                }
            }

            container[key] = value;
        }
    }
}

//***************************************************************************
// @brief 사용자 정의 객체 맵(CMap)을 XML 노드로 추가(직렬화)합니다.
// @param container 직렬화할 객체 맵 컨테이너
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename K, typename V>
inline void CRapidXMLUtil::AddObjectMap(const CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()));
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        xml_node<>* itemNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ItemName).c_str()));
        containerNode->append_node(itemNode);

        xml_node<>* node = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(MapKey).c_str()), _doc.allocate_string(TStringToUtf8(item.first).c_str()));
        itemNode->append_node(node);

        xml_node<>* classNode = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(MapValue).c_str()));
        itemNode->append_node(classNode);
        item.second.ToXML(classNode, _doc);
    }
}

//***************************************************************************
// @brief XML 노드로부터 사용자 정의 객체 맵(CMap)을 읽어옵니다(역직렬화).
// @param container [OUT] 읽어온 데이터를 저장할 객체 맵 컨테이너 참조
// @param parent 부모 노드 포인터
// @param ptszTagName 태그 이름
//***************************************************************************
template <typename K, typename V>
inline void CRapidXMLUtil::GetObjectMap(CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName)
{
    xml_node<>* containerNode = parent->first_node(TStringToUtf8(ptszTagName).c_str());
    if( containerNode )
    {
        for( xml_node<>* node = containerNode->first_node(TStringToUtf8(ItemName).c_str()); node; node = node->next_sibling(TStringToUtf8(ItemName).c_str()) )
        {
            K key;
            V value;

            xml_node<>* keyNode = node->first_node(TStringToUtf8(MapKey).c_str());
            if( keyNode )
            {
                key = Utf8ToTString(keyNode->value());
            }

            xml_node<>* valueNode = node->first_node(TStringToUtf8(MapValue).c_str());
            if( valueNode )
            {
                value.FromXML(valueNode);
            }

            container[key] = value;
        }
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
    if( node ) value = static_cast<T>(std::stod(node->value()));
}

//***************************************************************************
// @brief 다양한 타입의 객체/컨테이너를 XML 구조로 변환하여 문자열로 반환합니다.
// @param nodeName 노드 이름
// @param obj 변환할 객체
// @return 변환된 XML 문자열 (_tstring)
//***************************************************************************
template <typename T>
inline _tstring CRapidXMLUtil::ConvertToXML(const _tstring& nodeName, const T& obj)
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
        // 맵 처리
        using KeyType = typename T::key_type;
        using ValueType = typename T::mapped_type;
        if constexpr( std::is_same_v<KeyType, _tstring> )
        {
            // 키가 _tstring인 경우만 처리
            if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
            {
                AddMap(obj, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);               // 기본 자료형 맵
            }
            else
            {
                AddObjectMap(obj, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);     // 사용자 정의 클래스 맵
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

    std::ostringstream oss;
    oss << _doc;
    return Utf8ToTString(oss.str());
}

//***************************************************************************
// @brief XML 노드로부터 지정한 타입 T의 객체/컨테이너 값을 가져옵니다.
// @param nodeName 노드 이름
// @return 변환 성공 시 std::optional<T>, 실패 시 std::nullopt
//***************************************************************************
template <typename T>
inline std::optional<T> CRapidXMLUtil::ConvertFromXML(const _tstring& nodeName)
{
    T value;

    xml_node<char>* root = _doc.first_node();
    if( !root ) return std::nullopt;        // 노드가 없거나 값이 없으면 std::nullopt 반환

    if constexpr( is_vector<T>::value )
    {
        using ValueType = typename T::value_type;
        if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
        {
            GetVector<ValueType>(value, root, nodeName.size() > 0 ? nodeName.c_str() : VectorName);        // 기본 자료형 벡터
        }
        else
        {
            GetObjectVector<ValueType>(value, root, nodeName.size() > 0 ? nodeName.c_str() : VectorName);  // 사용자 정의 클래스 벡터
        }
    }
    else if constexpr( is_map<T>::value )
    {
        // 맵 처리
        using KeyType = typename T::key_type;
        using ValueType = typename T::mapped_type;
        if constexpr( std::is_same_v<KeyType, _tstring> )
        {
            // 키가 _tstring인 경우만 처리
            if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
            {
                GetMap<KeyType, ValueType>(value, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);               // 기본 자료형 맵
            }
            else
            {
                GetObjectMap<KeyType, ValueType>(value, root, nodeName.size() > 0 ? nodeName.c_str() : MapName);     // 사용자 정의 클래스 맵
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