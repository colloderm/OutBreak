# Codex 지침
절대 수정하지 않고 파악만하여 요청사항에 대하여 파악 작업 후 한국어 md 파일을 작성하여 제출한다.

# 시스템 설계 의도
중앙 집권 시스템으로 Overlord 하위 Viceroy 들에 Tick을 넣어준다.
그리고 Pooling을 하기 위하여 개체들을 Entity 형태로 관리하고 
삭제가 되면 RemoveAt을 동기화하여 마지막으로 추가된 Agent가 회수된 인덱스로 이동하여
조각화 문제를 해결한다. 또한 대부분 Instanced Static Mesh를 사용해 VAT로 시각화를 한다.
필요에 따라 Animation budget Allocator를 활용한 Skeletal Mesh로 시각화 해주어 예산을 주지만 그 예산도 제한하게 한다.
그리고 Agent에 대한 정보는 기능에 따라 전부 분산되어 있지만 한곳에서 집권적으로 정확하게 갱신되도록 작업해야한다.
그리고 World에는 시각화를 하지 않지만 Trnasform을 읽어 배치되고, 이동 중인 Actor가 있다 Capsule Compoent를 가지고 있으며 피격과, 충돌 등에 대한 이벤트 발생시 추가적인 Subsystem에 Queue로 삽입하여 처리를 병렬로 진행한다.