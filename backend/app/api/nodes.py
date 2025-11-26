from fastapi import APIRouter
from typing import List
from ..schemas import NodeStatus

router = APIRouter()


@router.get("/", response_model=List[NodeStatus])
async def list_nodes():
    # stub response
    return []
